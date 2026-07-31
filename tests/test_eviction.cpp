#include "store/shard.hpp"
#include "eviction/lru_policy.hpp"
#include "eviction/lfu_policy.hpp"
#include <iostream>
#include <cassert>

using namespace minicache::store;
using namespace minicache::eviction;

void testLRUEviction() {
    Shard shard;
    shard.setEvictionPolicy(std::make_unique<LRUPolicy>());
    shard.setMaxMemory(450); // Allows 3 keys (~360 bytes) to coexist

    // Insert 3 keys
    assert(shard.set("key1", std::string("val1")));
    assert(shard.set("key2", std::string("val2")));
    assert(shard.set("key3", std::string("val3")));

    // Access key1 so key1 becomes MRU; key2 is LRU
    assert(shard.get("key1").has_value());

    // Insert key4 to exceed 450 bytes and trigger eviction of LRU key (key2)
    assert(shard.set("key4", std::string("val4_large_value_to_force_eviction")));

    // key2 should have been evicted; key1 and key4 must remain present
    assert(!shard.get("key2").has_value());
    assert(shard.get("key1").has_value());
}

void testLFUEviction() {
    Shard shard;
    shard.setEvictionPolicy(std::make_unique<LFUPolicy>());
    shard.setMaxMemory(450); // Allows 3 keys (~360 bytes) to coexist

    // Insert 3 keys
    assert(shard.set("lfu1", std::string("v1")));
    assert(shard.set("lfu2", std::string("v2")));
    assert(shard.set("lfu3", std::string("v3")));

    // Access lfu1 multiple times, lfu3 multiple times
    for (int i = 0; i < 5; ++i) shard.get("lfu1");
    for (int i = 0; i < 3; ++i) shard.get("lfu3");
    // lfu2 has frequency 1 (lowest)

    // Insert lfu4 to exceed 450 bytes and force eviction
    assert(shard.set("lfu4", std::string("v4_force_evict_lowest_freq_key")));

    // lfu2 (lowest frequency) should be evicted; lfu1 must remain present
    assert(!shard.get("lfu2").has_value());
    assert(shard.get("lfu1").has_value());
}

int main() {
    std::cout << "Running test_eviction..." << std::endl;
    testLRUEviction();
    testLFUEviction();
    std::cout << "ALL EVICTION TESTS PASSED!" << std::endl;
    return 0;
}
