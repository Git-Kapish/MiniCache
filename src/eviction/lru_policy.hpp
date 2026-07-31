#pragma once

#include "eviction/eviction_policy.hpp"
#include <list>
#include <unordered_map>

namespace minicache::eviction {

class LRUPolicy : public EvictionPolicy {
public:
    LRUPolicy() = default;
    ~LRUPolicy() override = default;

    void onAccess(const std::string& key) override;
    void onInsert(const std::string& key) override;
    void onDelete(const std::string& key) override;
    std::string selectVictim() override;

private:
    std::list<std::string> lruList_;
    std::unordered_map<std::string, std::list<std::string>::iterator> map_;
};

} // namespace minicache::eviction
