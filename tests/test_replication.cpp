#include "store/shard_router.hpp"
#include "replication/replication_streamer.hpp"
#include "replication/replica_client.hpp"
#include "command/dispatcher.hpp"
#include "net/tcp_server.hpp"
#include "protocol/resp_value.hpp"
#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>

using namespace minicache::store;
using namespace minicache::replication;
using namespace minicache::command;
using namespace minicache::protocol;
using namespace minicache::net;

void testLeaderFollowerStreaming() {
    uint16_t leaderPort = 6390;

    // Leader Setup
    ShardRouter leaderRouter(4, 0, "lru");
    ReplicationStreamer streamer;
    CommandDispatcher leaderDispatcher(leaderRouter, nullptr, &streamer, false);
    TcpServer leaderServer(leaderPort, leaderDispatcher);
    assert(leaderServer.start());

    // Follower Setup
    ShardRouter followerRouter(4, 0, "lru");
    CommandDispatcher followerDispatcher(followerRouter, nullptr, nullptr, true); // Read-Only
    ReplicaClient replicaClient("127.0.0.1", leaderPort, followerRouter);
    assert(replicaClient.start());

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 1. Issue Writes to Leader
    leaderDispatcher.dispatch(RespValue::makeArray({
        RespValue::makeBulkString("SET"),
        RespValue::makeBulkString("repl_key"),
        RespValue::makeBulkString("repl_val")
    }));

    leaderDispatcher.dispatch(RespValue::makeArray({
        RespValue::makeBulkString("HSET"),
        RespValue::makeBulkString("repl_hash"),
        RespValue::makeBulkString("field1"),
        RespValue::makeBulkString("val1")
    }));

    leaderDispatcher.dispatch(RespValue::makeArray({
        RespValue::makeBulkString("LPUSH"),
        RespValue::makeBulkString("repl_list"),
        RespValue::makeBulkString("itemA")
    }));

    // Allow streaming to follower
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // 2. Verify Follower State
    RespValue res1 = followerDispatcher.dispatch(RespValue::makeArray({
        RespValue::makeBulkString("GET"),
        RespValue::makeBulkString("repl_key")
    }));
    assert(res1.type == RespType::BulkString);
    assert(std::get<std::string>(res1.data) == "repl_val");

    RespValue res2 = followerDispatcher.dispatch(RespValue::makeArray({
        RespValue::makeBulkString("HGET"),
        RespValue::makeBulkString("repl_hash"),
        RespValue::makeBulkString("field1")
    }));
    assert(res2.type == RespType::BulkString);
    assert(std::get<std::string>(res2.data) == "val1");

    RespValue res3 = followerDispatcher.dispatch(RespValue::makeArray({
        RespValue::makeBulkString("LLEN"),
        RespValue::makeBulkString("repl_list")
    }));
    assert(res3.type == RespType::Integer);
    assert(std::get<int64_t>(res3.data) == 1);

    // 3. Verify Follower Read-Only Enforcement
    RespValue failWrite = followerDispatcher.dispatch(RespValue::makeArray({
        RespValue::makeBulkString("SET"),
        RespValue::makeBulkString("direct_write"),
        RespValue::makeBulkString("forbidden")
    }));
    assert(failWrite.type == RespType::Error);
    assert(std::get<std::string>(failWrite.data).find("READONLY") != std::string::npos);

    replicaClient.stop();
    leaderServer.stop();
    std::cout << "Replication streaming & Read-Only enforcement verified!" << std::endl;
}

int main() {
    std::cout << "Running test_replication..." << std::endl;
    testLeaderFollowerStreaming();
    std::cout << "ALL REPLICATION TESTS PASSED!" << std::endl;
    return 0;
}
