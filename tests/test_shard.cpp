#include "store/shard.hpp"
#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>

using namespace minicache::store;

void testBasicSetGet() {
    Shard shard;
    assert(shard.set("key1", std::string("value1")));
    
    auto valOpt = shard.get("key1");
    assert(valOpt.has_value());
    assert(std::holds_alternative<std::string>(valOpt.value()));
    assert(std::get<std::string>(valOpt.value()) == "value1");

    assert(shard.exists("key1"));
    assert(shard.size() == 1);

    assert(shard.del("key1"));
    assert(!shard.get("key1").has_value());
    assert(!shard.exists("key1"));
    assert(shard.size() == 0);
}

void testIncrBy() {
    Shard shard;
    auto val1 = shard.incrBy("counter", 1);
    assert(val1.has_value());
    assert(val1.value() == 1);

    auto val2 = shard.incrBy("counter", 5);
    assert(val2.has_value());
    assert(val2.value() == 6);

    auto val3 = shard.incrBy("counter", -2);
    assert(val3.has_value());
    assert(val3.value() == 4);

    auto getOpt = shard.get("counter");
    assert(getOpt.has_value());
    assert(std::get<std::string>(getOpt.value()) == "4");
}

void testLazyExpiration() {
    Shard shard;
    // Set key with 100ms TTL
    assert(shard.set("tempKey", std::string("tempVal"), 100));
    assert(shard.exists("tempKey"));
    
    int64_t initialTtl = shard.ttl("tempKey");
    assert(initialTtl >= 0);

    // Sleep for 150ms
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Lazy expiration on read
    assert(!shard.exists("tempKey"));
    assert(!shard.get("tempKey").has_value());
    assert(shard.ttl("tempKey") == -2);
}

void testPersist() {
    Shard shard;
    assert(shard.set("pKey", std::string("pVal"), 5000)); // 5s TTL
    assert(shard.ttl("pKey") >= 0);

    assert(shard.persist("pKey"));
    assert(shard.ttl("pKey") == -1); // No TTL
}

int main() {
    std::cout << "Running test_shard..." << std::endl;
    testBasicSetGet();
    testIncrBy();
    testLazyExpiration();
    testPersist();
    std::cout << "ALL SHARD TESTS PASSED!" << std::endl;
    return 0;
}
