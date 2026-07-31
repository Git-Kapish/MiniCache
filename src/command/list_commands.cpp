#include "command/list_commands.hpp"

namespace minicache::command {

protocol::RespValue LPushCommand::execute(store::Shard& shard) {
    auto resOpt = shard.lpush(key_, values_);
    if (!resOpt.has_value()) {
        return protocol::RespValue::makeError("WRONGTYPE Operation against a key holding the wrong kind of value");
    }
    return protocol::RespValue::makeInteger(resOpt.value());
}

protocol::RespValue RPushCommand::execute(store::Shard& shard) {
    auto resOpt = shard.rpush(key_, values_);
    if (!resOpt.has_value()) {
        return protocol::RespValue::makeError("WRONGTYPE Operation against a key holding the wrong kind of value");
    }
    return protocol::RespValue::makeInteger(resOpt.value());
}

protocol::RespValue LPopCommand::execute(store::Shard& shard) {
    auto resOpt = shard.lpop(key_);
    if (!resOpt.has_value()) {
        return protocol::RespValue::makeNullBulkString();
    }
    return protocol::RespValue::makeBulkString(resOpt.value());
}

protocol::RespValue RPopCommand::execute(store::Shard& shard) {
    auto resOpt = shard.rpop(key_);
    if (!resOpt.has_value()) {
        return protocol::RespValue::makeNullBulkString();
    }
    return protocol::RespValue::makeBulkString(resOpt.value());
}

protocol::RespValue LRangeCommand::execute(store::Shard& shard) {
    auto resOpt = shard.lrange(key_, start_, stop_);
    if (!resOpt.has_value()) {
        return protocol::RespValue::makeError("WRONGTYPE Operation against a key holding the wrong kind of value");
    }
    std::vector<protocol::RespValue> arrayElems;
    arrayElems.reserve(resOpt.value().size());
    for (const auto& item : resOpt.value()) {
        arrayElems.push_back(protocol::RespValue::makeBulkString(item));
    }
    return protocol::RespValue::makeArray(std::move(arrayElems));
}

protocol::RespValue LLenCommand::execute(store::Shard& shard) {
    auto resOpt = shard.llen(key_);
    if (!resOpt.has_value()) {
        return protocol::RespValue::makeError("WRONGTYPE Operation against a key holding the wrong kind of value");
    }
    return protocol::RespValue::makeInteger(resOpt.value());
}

} // namespace minicache::command
