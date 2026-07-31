#pragma once

#include "store/shard.hpp"
#include "store/consistent_hash_ring.hpp"
#include <vector>
#include <memory>
#include <string>

namespace minicache::store {

enum class ShardingMode {
    Modulo,
    ConsistentHash
};

class ShardRouter {
public:
    ShardRouter(size_t numShards = 4, size_t totalMaxMemoryBytes = 0, const std::string& evictionPolicyName = "lru", ShardingMode mode = ShardingMode::Modulo);
    ~ShardRouter() = default;

    ShardRouter(const ShardRouter&) = delete;
    ShardRouter& operator=(const ShardRouter&) = delete;

    size_t getShardIndex(const std::string& key) const;
    Shard& getShard(const std::string& key);
    Shard& getShardByIndex(size_t index);
    size_t numShards() const;

    size_t getTotalSize();
    size_t getTotalMemoryUsage();

    ShardingMode getShardingMode() const { return mode_; }

private:
    std::vector<std::unique_ptr<Shard>> shards_;
    ShardingMode mode_;
    ConsistentHashRing hashRing_;
};

} // namespace minicache::store
