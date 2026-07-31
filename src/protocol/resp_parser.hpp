#pragma once

#include "protocol/resp_value.hpp"
#include <string>
#include <string_view>
#include <cstddef>

namespace minicache::protocol {

enum class ParseResult {
    Success,
    Incomplete,
    Error
};

class RespParser {
public:
    static ParseResult parse(std::string_view buffer, RespValue& outValue, size_t& bytesConsumed);

private:
    static ParseResult parseInternal(std::string_view buffer, RespValue& outValue, size_t& bytesConsumed);
};

} // namespace minicache::protocol
