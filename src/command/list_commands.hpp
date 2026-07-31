#pragma once

#include "command/command.hpp"
#include <string>
#include <vector>

namespace minicache::command {

class LPushCommand : public Command {
public:
    LPushCommand(std::string key, std::vector<std::string> values)
        : key_(std::move(key)), values_(std::move(values)) {}

    protocol::RespValue execute(store::Shard& shard) override;
    std::string getKey() const override { return key_; }
    bool isWriteCommand() const override { return true; }

private:
    std::string key_;
    std::vector<std::string> values_;
};

class RPushCommand : public Command {
public:
    RPushCommand(std::string key, std::vector<std::string> values)
        : key_(std::move(key)), values_(std::move(values)) {}

    protocol::RespValue execute(store::Shard& shard) override;
    std::string getKey() const override { return key_; }
    bool isWriteCommand() const override { return true; }

private:
    std::string key_;
    std::vector<std::string> values_;
};

class LPopCommand : public Command {
public:
    explicit LPopCommand(std::string key) : key_(std::move(key)) {}

    protocol::RespValue execute(store::Shard& shard) override;
    std::string getKey() const override { return key_; }
    bool isWriteCommand() const override { return true; }

private:
    std::string key_;
};

class RPopCommand : public Command {
public:
    explicit RPopCommand(std::string key) : key_(std::move(key)) {}

    protocol::RespValue execute(store::Shard& shard) override;
    std::string getKey() const override { return key_; }
    bool isWriteCommand() const override { return true; }

private:
    std::string key_;
};

class LRangeCommand : public Command {
public:
    LRangeCommand(std::string key, int64_t start, int64_t stop)
        : key_(std::move(key)), start_(start), stop_(stop) {}

    protocol::RespValue execute(store::Shard& shard) override;
    std::string getKey() const override { return key_; }
    bool isWriteCommand() const override { return false; }

private:
    std::string key_;
    int64_t start_;
    int64_t stop_;
};

class LLenCommand : public Command {
public:
    explicit LLenCommand(std::string key) : key_(std::move(key)) {}

    protocol::RespValue execute(store::Shard& shard) override;
    std::string getKey() const override { return key_; }
    bool isWriteCommand() const override { return false; }

private:
    std::string key_;
};

} // namespace minicache::command
