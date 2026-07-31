#include "store/shard_router.hpp"
#include "eviction/lru_policy.hpp"
#include "eviction/lfu_policy.hpp"
#include "eviction/no_eviction_policy.hpp"
#include <functional>

namespace minicache::store {

ShardRouter::ShardRouter(size_t numShards, size_t totalMaxMemoryBytes, const std::string& evictionPolicyName, ShardingMode mode)
    : mode_(mode), hashRing_(numShards, 100) {
    if (numShards == 0) {
        numShards = 1;
    }

    size_t perShardMaxMem = (totalMaxMemoryBytes > 0) ? (totalMaxMemoryBytes / numShards) : 0;

    shards_.reserve(numShards);
    for (size_t i = 0; i < numShards; ++i) {
        auto shard = std::make_unique<Shard>();
        shard->setMaxMemory(perShardMaxMem);

        if (evictionPolicyName == "lru") {
            shard->setEvictionPolicy(std::make_unique<eviction::LRUPolicy>());
        } else if (evictionPolicyName == "lfu") {
            shard->setEvictionPolicy(std::make_unique<eviction::LFUPolicy>());
        } else {
            shard->setEvictionPolicy(std::make_unique<eviction::NoEvictionPolicy>());
        }

        shards_.push_back(std::move(shard));
    }
}

size_t ShardRouter::getShardIndex(const std::string& key) const {
    if (mode_ == ShardingMode::ConsistentHash) {
        return hashRing_.getShardIndex(key);
    }
    std::hash<std::string> hasher;
    return hasher(key) % shards_.size();
}

Shard& ShardRouter::getShard(const std::string& key) {
    return *shards_[getShardIndex(key)];
}

Shard& ShardRouter::getShardByIndex(size_t index) {
    return *shards_[index % shards_.size()];
}

size_t ShardRouter::numShards() const {
    return shards_.size();
}

size_t ShardRouter::getTotalSize() {
    size_t total = 0;
    for (const auto& shard : shards_) {
        total += shard->size();
    }
    return total;
}

size_t ShardRouter::getTotalMemoryUsage() {
    size_t total = 0;
    for (const auto& shard : shards_) {
        total += shard->getMemoryUsage();
    }
    return total;
}

} // namespace minicache::store
