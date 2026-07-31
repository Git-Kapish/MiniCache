#pragma once

#include "command/command.hpp"
#include <string>
#include <vector>
#include <optional>

namespace minicache::command {

class SetCommand : public Command {
public:
    SetCommand(std::string key, std::string value, std::optional<uint64_t> ttlMs = std::nullopt)
        : key_(std::move(key)), value_(std::move(value)), ttlMs_(ttlMs) {}

    protocol::RespValue execute(store::Shard& shard) override;
    std::string getKey() const override { return key_; }
    bool isWriteCommand() const override { return true; }

private:
    std::string key_;
    std::string value_;
    std::optional<uint64_t> ttlMs_;
};

class GetCommand : public Command {
public:
    explicit GetCommand(std::string key) : key_(std::move(key)) {}

    protocol::RespValue execute(store::Shard& shard) override;
    std::string getKey() const override { return key_; }
    bool isWriteCommand() const override { return false; }

private:
    std::string key_;
};

class DelCommand : public Command {
public:
    explicit DelCommand(std::vector<std::string> keys) : keys_(std::move(keys)) {}

    protocol::RespValue execute(store::Shard& shard) override;
    std::string getKey() const override { return keys_.empty() ? "" : keys_[0]; }
    const std::vector<std::string>& getKeys() const { return keys_; }
    bool isWriteCommand() const override { return true; }

private:
    std::vector<std::string> keys_;
};

class ExistsCommand : public Command {
public:
    explicit ExistsCommand(std::vector<std::string> keys) : keys_(std::move(keys)) {}

    protocol::RespValue execute(store::Shard& shard) override;
    std::string getKey() const override { return keys_.empty() ? "" : keys_[0]; }
    const std::vector<std::string>& getKeys() const { return keys_; }
    bool isWriteCommand() const override { return false; }

private:
    std::vector<std::string> keys_;
};

class ExpireCommand : public Command {
public:
    ExpireCommand(std::string key, uint64_t ttlSeconds)
        : key_(std::move(key)), ttlSeconds_(ttlSeconds) {}

    protocol::RespValue execute(store::Shard& shard) override;
    std::string getKey() const override { return key_; }
    bool isWriteCommand() const override { return true; }

private:
    std::string key_;
    uint64_t ttlSeconds_;
};

class TtlCommand : public Command {
public:
    explicit TtlCommand(std::string key) : key_(std::move(key)) {}

    protocol::RespValue execute(store::Shard& shard) override;
    std::string getKey() const override { return key_; }
    bool isWriteCommand() const override { return false; }

private:
    std::string key_;
};

class PersistCommand : public Command {
public:
    explicit PersistCommand(std::string key) : key_(std::move(key)) {}

    protocol::RespValue execute(store::Shard& shard) override;
    std::string getKey() const override { return key_; }
    bool isWriteCommand() const override { return true; }

private:
    std::string key_;
};

class IncrByCommand : public Command {
public:
    IncrByCommand(std::string key, int64_t delta)
        : key_(std::move(key)), delta_(delta) {}

    protocol::RespValue execute(store::Shard& shard) override;
    std::string getKey() const override { return key_; }
    bool isWriteCommand() const override { return true; }

private:
    std::string key_;
    int64_t delta_;
};

class PingCommand : public Command {
public:
    explicit PingCommand(std::string message = "") : message_(std::move(message)) {}

    protocol::RespValue execute(store::Shard& shard) override;
    bool isWriteCommand() const override { return false; }

private:
    std::string message_;
};

} // namespace minicache::command
