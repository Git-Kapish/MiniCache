#include "command/hash_commands.hpp"

namespace minicache::command {

protocol::RespValue HSetCommand::execute(store::Shard& shard) {
    auto resOpt = shard.hset(key_, fieldValues_);
    if (!resOpt.has_value()) {
        return protocol::RespValue::makeError("WRONGTYPE Operation against a key holding the wrong kind of value");
    }
    return protocol::RespValue::makeInteger(resOpt.value());
}

protocol::RespValue HGetCommand::execute(store::Shard& shard) {
    auto resOpt = shard.hget(key_, field_);
    if (!resOpt.has_value()) {
        return protocol::RespValue::makeNullBulkString();
    }
    return protocol::RespValue::makeBulkString(resOpt.value());
}

protocol::RespValue HDelCommand::execute(store::Shard& shard) {
    auto resOpt = shard.hdel(key_, fields_);
    if (!resOpt.has_value()) {
        return protocol::RespValue::makeError("WRONGTYPE Operation against a key holding the wrong kind of value");
    }
    return protocol::RespValue::makeInteger(resOpt.value());
}

protocol::RespValue HGetAllCommand::execute(store::Shard& shard) {
    auto resOpt = shard.hgetall(key_);
    if (!resOpt.has_value()) {
        return protocol::RespValue::makeError("WRONGTYPE Operation against a key holding the wrong kind of value");
    }
    std::vector<protocol::RespValue> arrayElems;
    arrayElems.reserve(resOpt.value().size() * 2);
    for (const auto& [field, val] : resOpt.value()) {
        arrayElems.push_back(protocol::RespValue::makeBulkString(field));
        arrayElems.push_back(protocol::RespValue::makeBulkString(val));
    }
    return protocol::RespValue::makeArray(std::move(arrayElems));
}

protocol::RespValue HExistsCommand::execute(store::Shard& shard) {
    auto resOpt = shard.hexists(key_, field_);
    if (!resOpt.has_value()) {
        return protocol::RespValue::makeError("WRONGTYPE Operation against a key holding the wrong kind of value");
    }
    return protocol::RespValue::makeInteger(resOpt.value() ? 1 : 0);
}

} // namespace minicache::command
