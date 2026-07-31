#include "store/shard_router.hpp"
#include "persistence/aof_writer.hpp"
#include "persistence/recovery.hpp"
#include "command/dispatcher.hpp"
#include "protocol/resp_value.hpp"
#include <iostream>
#include <cassert>
#include <fstream>
#include <cstdio>

using namespace minicache::store;
using namespace minicache::persistence;
using namespace minicache::command;
using namespace minicache::protocol;

void testAofRecovery() {
    const std::string aofFile = "test_persistence.aof";
    std::remove(aofFile.c_str());

    {
        AofWriter aofWriter(aofFile, FsyncPolicy::Always);
        assert(aofWriter.open());

        ShardRouter router(4, 0, "lru");
        CommandDispatcher dispatcher(router, &aofWriter);

        // 1. SET foo bar
        dispatcher.dispatch(RespValue::makeArray({
            RespValue::makeBulkString("SET"),
            RespValue::makeBulkString("foo"),
            RespValue::makeBulkString("bar")
        }));

        // 2. HSET user:1 name Alice
        dispatcher.dispatch(RespValue::makeArray({
            RespValue::makeBulkString("HSET"),
            RespValue::makeBulkString("user:1"),
            RespValue::makeBulkString("name"),
            RespValue::makeBulkString("Alice")
        }));

        // 3. LPUSH mylist item1 item2
        dispatcher.dispatch(RespValue::makeArray({
            RespValue::makeBulkString("LPUSH"),
            RespValue::makeBulkString("mylist"),
            RespValue::makeBulkString("item1"),
            RespValue::makeBulkString("item2")
        }));

        // 4. SADD myset m1 m2
        dispatcher.dispatch(RespValue::makeArray({
            RespValue::makeBulkString("SADD"),
            RespValue::makeBulkString("myset"),
            RespValue::makeBulkString("m1"),
            RespValue::makeBulkString("m2")
        }));

        // 5. INCR counter
        dispatcher.dispatch(RespValue::makeArray({
            RespValue::makeBulkString("INCR"),
            RespValue::makeBulkString("counter")
        }));

        // 6. EXPIRE foo 100
        dispatcher.dispatch(RespValue::makeArray({
            RespValue::makeBulkString("EXPIRE"),
            RespValue::makeBulkString("foo"),
            RespValue::makeBulkString("100")
        }));

        aofWriter.close(); // Simulates process shutdown / crash after writes
    }

    // Recover into clean ShardRouter
    ShardRouter recoveredRouter(4, 0, "lru");
    size_t replayed = Recovery::recover(recoveredRouter, "", aofFile);
    std::cout << "Replayed " << replayed << " AOF commands on recovery." << std::endl;
    assert(replayed >= 6);

    CommandDispatcher recoveredDispatcher(recoveredRouter);

    // Verify SET foo -> bar
    RespValue res1 = recoveredDispatcher.dispatch(RespValue::makeArray({
        RespValue::makeBulkString("GET"),
        RespValue::makeBulkString("foo")
    }));
    assert(res1.type == RespType::BulkString);
    assert(std::get<std::string>(res1.data) == "bar");

    // Verify HGET user:1 name -> Alice
    RespValue res2 = recoveredDispatcher.dispatch(RespValue::makeArray({
        RespValue::makeBulkString("HGET"),
        RespValue::makeBulkString("user:1"),
        RespValue::makeBulkString("name")
    }));
    assert(res2.type == RespType::BulkString);
    assert(std::get<std::string>(res2.data) == "Alice");

    // Verify LLEN mylist -> 2
    RespValue res3 = recoveredDispatcher.dispatch(RespValue::makeArray({
        RespValue::makeBulkString("LLEN"),
        RespValue::makeBulkString("mylist")
    }));
    assert(res3.type == RespType::Integer);
    assert(std::get<int64_t>(res3.data) == 2);

    // Verify SISMEMBER myset m1 -> 1
    RespValue res4 = recoveredDispatcher.dispatch(RespValue::makeArray({
        RespValue::makeBulkString("SISMEMBER"),
        RespValue::makeBulkString("myset"),
        RespValue::makeBulkString("m1")
    }));
    assert(res4.type == RespType::Integer);
    assert(std::get<int64_t>(res4.data) == 1);

    // Verify GET counter -> 1
    RespValue res5 = recoveredDispatcher.dispatch(RespValue::makeArray({
        RespValue::makeBulkString("GET"),
        RespValue::makeBulkString("counter")
    }));
    assert(res5.type == RespType::BulkString);
    assert(std::get<std::string>(res5.data) == "1");

    // Verify TTL foo > 0
    RespValue res6 = recoveredDispatcher.dispatch(RespValue::makeArray({
        RespValue::makeBulkString("TTL"),
        RespValue::makeBulkString("foo")
    }));
    assert(res6.type == RespType::Integer);
    assert(std::get<int64_t>(res6.data) > 0);

    std::remove(aofFile.c_str());
}

int main() {
    std::cout << "Running test_persistence..." << std::endl;
    testAofRecovery();
    std::cout << "ALL PERSISTENCE TESTS PASSED!" << std::endl;
    return 0;
}
