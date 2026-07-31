#include "command/commands.hpp"

namespace minicache::command {

protocol::RespValue SetCommand::execute(store::Shard& shard) {
    shard.set(key_, value_, ttlMs_);
    return protocol::RespValue::makeSimpleString("OK");
}

protocol::RespValue GetCommand::execute(store::Shard& shard) {
    auto valOpt = shard.get(key_);
    if (!valOpt.has_value()) {
        return protocol::RespValue::makeNullBulkString();
    }
    if (std::holds_alternative<std::string>(valOpt.value())) {
        return protocol::RespValue::makeBulkString(std::get<std::string>(valOpt.value()));
    }
    return protocol::RespValue::makeError("WRONGTYPE Operation against a key holding the wrong kind of value");
}

protocol::RespValue DelCommand::execute(store::Shard& shard) {
    int64_t count = 0;
    for (const auto& key : keys_) {
        if (shard.del(key)) {
            count++;
        }
    }
    return protocol::RespValue::makeInteger(count);
}

protocol::RespValue ExistsCommand::execute(store::Shard& shard) {
    int64_t count = 0;
    for (const auto& key : keys_) {
        if (shard.exists(key)) {
            count++;
        }
    }
    return protocol::RespValue::makeInteger(count);
}

protocol::RespValue ExpireCommand::execute(store::Shard& shard) {
    bool res = shard.expire(key_, ttlSeconds_ * 1000);
    return protocol::RespValue::makeInteger(res ? 1 : 0);
}

protocol::RespValue TtlCommand::execute(store::Shard& shard) {
    int64_t ttlSec = shard.ttl(key_);
    return protocol::RespValue::makeInteger(ttlSec);
}

protocol::RespValue PersistCommand::execute(store::Shard& shard) {
    bool res = shard.persist(key_);
    return protocol::RespValue::makeInteger(res ? 1 : 0);
}

protocol::RespValue IncrByCommand::execute(store::Shard& shard) {
    auto resOpt = shard.incrBy(key_, delta_);
    if (!resOpt.has_value()) {
        return protocol::RespValue::makeError("ERR value is not an integer or out of range");
    }
    return protocol::RespValue::makeInteger(resOpt.value());
}

protocol::RespValue PingCommand::execute(store::Shard& /*shard*/) {
    if (message_.empty()) {
        return protocol::RespValue::makeSimpleString("PONG");
    }
    return protocol::RespValue::makeBulkString(message_);
}

} // namespace minicache::command
