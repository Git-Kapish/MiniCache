#pragma once

#include "eviction/eviction_policy.hpp"

namespace minicache::eviction {

class NoEvictionPolicy : public EvictionPolicy {
public:
    NoEvictionPolicy() = default;
    ~NoEvictionPolicy() override = default;

    void onAccess(const std::string& /*key*/) override {}
    void onInsert(const std::string& /*key*/) override {}
    void onDelete(const std::string& /*key*/) override {}
    std::string selectVictim() override {
        return "";
    }
};

} // namespace minicache::eviction
