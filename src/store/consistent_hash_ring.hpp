#pragma once

#include <string>
#include <map>
#include <vector>
#include <cstdint>
#include <cstddef>

namespace minicache::store {

class ConsistentHashRing {
public:
    explicit ConsistentHashRing(size_t numShards = 4, size_t vnodesPerShard = 100);
    ~ConsistentHashRing() = default;

    size_t getShardIndex(const std::string& key) const;
    size_t numShards() const { return numShards_; }

    static uint32_t hashString(const std::string& str);

private:
    void buildRing();

    size_t numShards_;
    size_t vnodesPerShard_;
    std::map<uint32_t, size_t> ring_; // hash position -> shard index
};

} // namespace minicache::store
