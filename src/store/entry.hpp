#pragma once

#include "store/value.hpp"
#include <optional>
#include <cstdint>
#include <chrono>

namespace minicache::store {

inline uint64_t getCurrentTimeMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

struct Entry {
    Value value;
    std::optional<uint64_t> expiresAtMs{std::nullopt};
    uint64_t lastAccessMs{0};
    uint32_t accessFreq{1};

    bool isExpired(uint64_t nowMs = getCurrentTimeMs()) const {
        if (!expiresAtMs.has_value()) {
            return false;
        }
        return nowMs >= expiresAtMs.value();
    }
};

} // namespace minicache::store
