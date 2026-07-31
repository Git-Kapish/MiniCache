#include "command/set_commands.hpp"

namespace minicache::command {

protocol::RespValue SAddCommand::execute(store::Shard& shard) {
    auto resOpt = shard.sadd(key_, members_);
    if (!resOpt.has_value()) {
        return protocol::RespValue::makeError("WRONGTYPE Operation against a key holding the wrong kind of value");
    }
    return protocol::RespValue::makeInteger(resOpt.value());
}

protocol::RespValue SRemCommand::execute(store::Shard& shard) {
    auto resOpt = shard.srem(key_, members_);
    if (!resOpt.has_value()) {
        return protocol::RespValue::makeError("WRONGTYPE Operation against a key holding the wrong kind of value");
    }
    return protocol::RespValue::makeInteger(resOpt.value());
}

protocol::RespValue SIsMemberCommand::execute(store::Shard& shard) {
    auto resOpt = shard.sismember(key_, member_);
    if (!resOpt.has_value()) {
        return protocol::RespValue::makeError("WRONGTYPE Operation against a key holding the wrong kind of value");
    }
    return protocol::RespValue::makeInteger(resOpt.value() ? 1 : 0);
}

protocol::RespValue SMembersCommand::execute(store::Shard& shard) {
    auto resOpt = shard.smembers(key_);
    if (!resOpt.has_value()) {
        return protocol::RespValue::makeError("WRONGTYPE Operation against a key holding the wrong kind of value");
    }
    std::vector<protocol::RespValue> arrayElems;
    arrayElems.reserve(resOpt.value().size());
    for (const auto& m : resOpt.value()) {
        arrayElems.push_back(protocol::RespValue::makeBulkString(m));
    }
    return protocol::RespValue::makeArray(std::move(arrayElems));
}

} // namespace minicache::command
