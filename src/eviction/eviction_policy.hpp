#pragma once

#include <string>

namespace minicache::eviction {

class EvictionPolicy {
public:
    virtual ~EvictionPolicy() = default;

    virtual void onAccess(const std::string& key) = 0;
    virtual void onInsert(const std::string& key) = 0;
    virtual void onDelete(const std::string& key) = 0;
    virtual std::string selectVictim() = 0;
};

} // namespace minicache::eviction
