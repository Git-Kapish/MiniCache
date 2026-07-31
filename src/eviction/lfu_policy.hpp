#pragma once

#include "eviction/eviction_policy.hpp"
#include <unordered_map>
#include <map>
#include <set>
#include <cstdint>

namespace minicache::eviction {

class LFUPolicy : public EvictionPolicy {
public:
    LFUPolicy() = default;
    ~LFUPolicy() override = default;

    void onAccess(const std::string& key) override;
    void onInsert(const std::string& key) override;
    void onDelete(const std::string& key) override;
    std::string selectVictim() override;

private:
    std::unordered_map<std::string, uint32_t> keyFreq_;
    std::map<uint32_t, std::set<std::string>> freqBuckets_;
};

} // namespace minicache::eviction
