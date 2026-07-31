#include "protocol/resp_parser.hpp"
#include <charconv>
#include <system_error>

namespace minicache::protocol {

namespace {

bool findCRLF(std::string_view sv, size_t offset, size_t& crlfPos) {
    size_t pos = sv.find("\r\n", offset);
    if (pos == std::string_view::npos) {
        return false;
    }
    crlfPos = pos;
    return true;
}

} // namespace

ParseResult RespParser::parse(std::string_view buffer, RespValue& outValue, size_t& bytesConsumed) {
    bytesConsumed = 0;
    if (buffer.empty()) {
        return ParseResult::Incomplete;
    }
    return parseInternal(buffer, outValue, bytesConsumed);
}

ParseResult RespParser::parseInternal(std::string_view buffer, RespValue& outValue, size_t& bytesConsumed) {
    if (buffer.empty()) {
        return ParseResult::Incomplete;
    }

    char prefix = buffer[0];

    switch (prefix) {
        case '+': { // Simple String
            size_t crlfPos = 0;
            if (!findCRLF(buffer, 1, crlfPos)) {
                return ParseResult::Incomplete;
            }
            std::string content(buffer.substr(1, crlfPos - 1));
            outValue = RespValue::makeSimpleString(std::move(content));
            bytesConsumed = crlfPos + 2;
            return ParseResult::Success;
        }
        case '-': { // Error
            size_t crlfPos = 0;
            if (!findCRLF(buffer, 1, crlfPos)) {
                return ParseResult::Incomplete;
            }
            std::string content(buffer.substr(1, crlfPos - 1));
            outValue = RespValue::makeError(std::move(content));
            bytesConsumed = crlfPos + 2;
            return ParseResult::Success;
        }
        case ':': { // Integer
            size_t crlfPos = 0;
            if (!findCRLF(buffer, 1, crlfPos)) {
                return ParseResult::Incomplete;
            }
            std::string_view numSv = buffer.substr(1, crlfPos - 1);
            int64_t val = 0;
            auto [ptr, ec] = std::from_chars(numSv.data(), numSv.data() + numSv.size(), val);
            if (ec != std::errc() || ptr != numSv.data() + numSv.size()) {
                return ParseResult::Error;
            }
            outValue = RespValue::makeInteger(val);
            bytesConsumed = crlfPos + 2;
            return ParseResult::Success;
        }
        case '$': { // Bulk String
            size_t crlfPos = 0;
            if (!findCRLF(buffer, 1, crlfPos)) {
                return ParseResult::Incomplete;
            }
            std::string_view lenSv = buffer.substr(1, crlfPos - 1);
            int64_t len = 0;
            auto [ptr, ec] = std::from_chars(lenSv.data(), lenSv.data() + lenSv.size(), len);
            if (ec != std::errc() || ptr != lenSv.data() + lenSv.size()) {
                return ParseResult::Error;
            }

            if (len == -1) { // Null Bulk String
                outValue = RespValue::makeNullBulkString();
                bytesConsumed = crlfPos + 2;
                return ParseResult::Success;
            }

            if (len < 0) {
                return ParseResult::Error;
            }

            size_t dataStart = crlfPos + 2;
            size_t dataEnd = dataStart + static_cast<size_t>(len);
            if (buffer.size() < dataEnd + 2) { // Need payload + CRLF
                return ParseResult::Incomplete;
            }

            if (buffer.substr(dataEnd, 2) != "\r\n") {
                return ParseResult::Error;
            }

            std::string content(buffer.substr(dataStart, static_cast<size_t>(len)));
            outValue = RespValue::makeBulkString(std::move(content));
            bytesConsumed = dataEnd + 2;
            return ParseResult::Success;
        }
        case '*': { // Array
            size_t crlfPos = 0;
            if (!findCRLF(buffer, 1, crlfPos)) {
                return ParseResult::Incomplete;
            }
            std::string_view countSv = buffer.substr(1, crlfPos - 1);
            int64_t count = 0;
            auto [ptr, ec] = std::from_chars(countSv.data(), countSv.data() + countSv.size(), count);
            if (ec != std::errc() || ptr != countSv.data() + countSv.size()) {
                return ParseResult::Error;
            }

            if (count == -1) { // Null Array
                outValue = RespValue::makeNullArray();
                bytesConsumed = crlfPos + 2;
                return ParseResult::Success;
            }

            if (count < 0) {
                return ParseResult::Error;
            }

            size_t currentOffset = crlfPos + 2;
            std::vector<RespValue> elements;
            elements.reserve(static_cast<size_t>(count));

            for (int64_t i = 0; i < count; ++i) {
                RespValue elem;
                size_t elemConsumed = 0;
                ParseResult res = parseInternal(buffer.substr(currentOffset), elem, elemConsumed);
                if (res != ParseResult::Success) {
                    return res; // Incomplete or Error
                }
                elements.push_back(std::move(elem));
                currentOffset += elemConsumed;
            }

            outValue = RespValue::makeArray(std::move(elements));
            bytesConsumed = currentOffset;
            return ParseResult::Success;
        }
        default:
            return ParseResult::Error;
    }
}

} // namespace minicache::protocol
