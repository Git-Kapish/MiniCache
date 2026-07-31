#include "eviction/lfu_policy.hpp"

namespace minicache::eviction {

void LFUPolicy::onAccess(const std::string& key) {
    auto it = keyFreq_.find(key);
    if (it != keyFreq_.end()) {
        uint32_t oldFreq = it->second;
        uint32_t newFreq = oldFreq + 1;

        freqBuckets_[oldFreq].erase(key);
        if (freqBuckets_[oldFreq].empty()) {
            freqBuckets_.erase(oldFreq);
        }

        it->second = newFreq;
        freqBuckets_[newFreq].insert(key);
    }
}

void LFUPolicy::onInsert(const std::string& key) {
    auto it = keyFreq_.find(key);
    if (it != keyFreq_.end()) {
        onAccess(key);
    } else {
        keyFreq_[key] = 1;
        freqBuckets_[1].insert(key);
    }
}

void LFUPolicy::onDelete(const std::string& key) {
    auto it = keyFreq_.find(key);
    if (it != keyFreq_.end()) {
        uint32_t freq = it->second;
        freqBuckets_[freq].erase(key);
        if (freqBuckets_[freq].empty()) {
            freqBuckets_.erase(freq);
        }
        keyFreq_.erase(it);
    }
}

std::string LFUPolicy::selectVictim() {
    if (freqBuckets_.empty()) {
        return "";
    }
    // Lowest frequency bucket is at beginning of std::map
    const auto& lowestBucket = freqBuckets_.begin()->second;
    if (lowestBucket.empty()) {
        return "";
    }
    return *lowestBucket.begin();
}

} // namespace minicache::eviction
