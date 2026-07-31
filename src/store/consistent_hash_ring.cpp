#include "store/consistent_hash_ring.hpp"

namespace minicache::store {

ConsistentHashRing::ConsistentHashRing(size_t numShards, size_t vnodesPerShard)
    : numShards_(numShards == 0 ? 1 : numShards), vnodesPerShard_(vnodesPerShard) {
    buildRing();
}

uint32_t ConsistentHashRing::hashString(const std::string& str) {
    uint32_t hash = 2166136261u;
    for (char c : str) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 16777619u;
    }
    return hash;
}

void ConsistentHashRing::buildRing() {
    ring_.clear();
    for (size_t s = 0; s < numShards_; ++s) {
        for (size_t v = 0; v < vnodesPerShard_; ++v) {
            std::string vnodeKey = "shard_" + std::to_string(s) + "_vnode_" + std::to_string(v);
            uint32_t hashVal = hashString(vnodeKey);
            ring_[hashVal] = s;
        }
    }
}

size_t ConsistentHashRing::getShardIndex(const std::string& key) const {
    if (ring_.empty()) {
        return 0;
    }

    uint32_t keyHash = hashString(key);
    auto it = ring_.lower_bound(keyHash);
    if (it == ring_.end()) {
        it = ring_.begin();
    }
    return it->second;
}

} // namespace minicache::store
