#include "store/shard_router.hpp"
#include "command/dispatcher.hpp"
#include "command/commands.hpp"
#include "protocol/resp_value.hpp"
#include <iostream>
#include <cassert>
#include <thread>
#include <vector>
#include <string>

using namespace minicache::store;
using namespace minicache::command;
using namespace minicache::protocol;

void testConcurrentIncrOnSharedKey() {
    constexpr int kThreads = 10;
    constexpr int kIterationsPerThread = 5000;

    ShardRouter router(4, 0, "lru");
    CommandDispatcher dispatcher(router);

    // Initial SET counter 0
    RespValue setReq = RespValue::makeArray({
        RespValue::makeBulkString("SET"),
        RespValue::makeBulkString("counter"),
        RespValue::makeBulkString("0")
    });
    dispatcher.dispatch(setReq);

    std::vector<std::thread> threads;
    threads.reserve(kThreads);

    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&dispatcher, kIterationsPerThread]() {
            RespValue incrReq = RespValue::makeArray({
                RespValue::makeBulkString("INCR"),
                RespValue::makeBulkString("counter")
            });
            for (int i = 0; i < kIterationsPerThread; ++i) {
                RespValue resp = dispatcher.dispatch(incrReq);
                assert(resp.type == RespType::Integer);
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    // Check final count
    RespValue getReq = RespValue::makeArray({
        RespValue::makeBulkString("GET"),
        RespValue::makeBulkString("counter")
    });
    RespValue finalResp = dispatcher.dispatch(getReq);

    assert(finalResp.type == RespType::BulkString);
    int64_t finalVal = std::stoll(std::get<std::string>(finalResp.data));
    int64_t expectedVal = static_cast<int64_t>(kThreads) * kIterationsPerThread;

    std::cout << "Concurrent INCR Result: " << finalVal << " (Expected: " << expectedVal << ")" << std::endl;
    assert(finalVal == expectedVal);
}

void testConcurrentShardedOperations() {
    constexpr int kThreads = 8;
    constexpr int kKeysPerThread = 1000;

    ShardRouter router(8, 0, "lru");
    CommandDispatcher dispatcher(router);

    std::vector<std::thread> threads;
    threads.reserve(kThreads);

    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&dispatcher, t, kKeysPerThread]() {
            for (int k = 0; k < kKeysPerThread; ++k) {
                std::string key = "key_thread_" + std::to_string(t) + "_" + std::to_string(k);
                std::string val = "val_" + std::to_string(k);

                RespValue setReq = RespValue::makeArray({
                    RespValue::makeBulkString("SET"),
                    RespValue::makeBulkString(key),
                    RespValue::makeBulkString(val)
                });
                RespValue setResp = dispatcher.dispatch(setReq);
                assert(setResp.type == RespType::SimpleString);

                RespValue getReq = RespValue::makeArray({
                    RespValue::makeBulkString("GET"),
                    RespValue::makeBulkString(key)
                });
                RespValue getResp = dispatcher.dispatch(getReq);
                assert(getResp.type == RespType::BulkString);
                assert(std::get<std::string>(getResp.data) == val);
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    std::cout << "Sharded operations test passed across 8 shards with " 
              << (kThreads * kKeysPerThread) << " keys!" << std::endl;
}

int main() {
    std::cout << "Running test_concurrency_stress..." << std::endl;
    testConcurrentIncrOnSharedKey();
    testConcurrentShardedOperations();
    std::cout << "ALL CONCURRENCY STRESS TESTS PASSED!" << std::endl;
    return 0;
}
