#pragma once

#include "protocol/resp_value.hpp"
#include <string>

namespace minicache::protocol {

class RespEncoder {
public:
    static std::string encode(const RespValue& value);
};

} // namespace minicache::protocol
