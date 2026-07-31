#pragma once

#include <string>
#include <vector>
#include <variant>

namespace minicache::protocol {

enum class RespType {
    SimpleString,
    Error,
    Integer,
    BulkString,
    Array,
    NullBulkString,
    NullArray
};

struct RespValue {
    RespType type{RespType::NullBulkString};
    std::variant<std::string, int64_t, std::vector<RespValue>> data;

    static RespValue makeSimpleString(std::string str) {
        RespValue val;
        val.type = RespType::SimpleString;
        val.data = std::move(str);
        return val;
    }

    static RespValue makeError(std::string err) {
        RespValue val;
        val.type = RespType::Error;
        val.data = std::move(err);
        return val;
    }

    static RespValue makeInteger(int64_t num) {
        RespValue val;
        val.type = RespType::Integer;
        val.data = num;
        return val;
    }

    static RespValue makeBulkString(std::string str) {
        RespValue val;
        val.type = RespType::BulkString;
        val.data = std::move(str);
        return val;
    }

    static RespValue makeNullBulkString() {
        RespValue val;
        val.type = RespType::NullBulkString;
        return val;
    }

    static RespValue makeArray(std::vector<RespValue> elements) {
        RespValue val;
        val.type = RespType::Array;
        val.data = std::move(elements);
        return val;
    }

    static RespValue makeNullArray() {
        RespValue val;
        val.type = RespType::NullArray;
        return val;
    }
};

} // namespace minicache::protocol
