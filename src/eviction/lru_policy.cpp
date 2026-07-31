#include "eviction/lru_policy.hpp"

namespace minicache::eviction {

void LRUPolicy::onAccess(const std::string& key) {
    auto it = map_.find(key);
    if (it != map_.end()) {
        lruList_.splice(lruList_.begin(), lruList_, it->second);
    }
}

void LRUPolicy::onInsert(const std::string& key) {
    auto it = map_.find(key);
    if (it != map_.end()) {
        lruList_.splice(lruList_.begin(), lruList_, it->second);
    } else {
        lruList_.push_front(key);
        map_[key] = lruList_.begin();
    }
}

void LRUPolicy::onDelete(const std::string& key) {
    auto it = map_.find(key);
    if (it != map_.end()) {
        lruList_.erase(it->second);
        map_.erase(it);
    }
}

std::string LRUPolicy::selectVictim() {
    if (lruList_.empty()) {
        return "";
    }
    return lruList_.back();
}

} // namespace minicache::eviction
