#include "store/consistent_hash_ring.hpp"
#include "store/shard_router.hpp"
#include <iostream>
#include <cassert>
#include <unordered_map>
#include <string>

using namespace minicache::store;

void testRingDistribution() {
    ConsistentHashRing ring(4, 100);
    std::unordered_map<size_t, size_t> counts;

    constexpr size_t kTotalKeys = 10000;
    for (size_t i = 0; i < kTotalKeys; ++i) {
        std::string key = "key_user_" + std::to_string(i);
        size_t shardIdx = ring.getShardIndex(key);
        counts[shardIdx]++;
    }

    std::cout << "Consistent Hash Ring Distribution across 4 shards (10,000 keys):" << std::endl;
    for (size_t s = 0; s < 4; ++s) {
        std::cout << "  Shard " << s << ": " << counts[s] << " keys ("
                  << (counts[s] * 100.0 / kTotalKeys) << "%)" << std::endl;
        assert(counts[s] > 1500); // Expect ~25% per shard (+/- variance)
    }
}

void testShardRouterConsistentHash() {
    ShardRouter router(4, 0, "lru", ShardingMode::ConsistentHash);
    assert(router.getShardingMode() == ShardingMode::ConsistentHash);

    assert(router.getShard("user:100").set("user:100", std::string("Alice")));
    auto valOpt = router.getShard("user:100").get("user:100");
    assert(valOpt.has_value());
    assert(std::get<std::string>(valOpt.value()) == "Alice");
}

int main() {
    std::cout << "Running test_consistent_hash..." << std::endl;
    testRingDistribution();
    testShardRouterConsistentHash();
    std::cout << "ALL CONSISTENT HASH TESTS PASSED!" << std::endl;
    return 0;
}
