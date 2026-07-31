#pragma once

#include "command/command.hpp"
#include <string>
#include <vector>
#include <utility>

namespace minicache::command {

class HSetCommand : public Command {
public:
    HSetCommand(std::string key, std::vector<std::pair<std::string, std::string>> fieldValues)
        : key_(std::move(key)), fieldValues_(std::move(fieldValues)) {}

    protocol::RespValue execute(store::Shard& shard) override;
    std::string getKey() const override { return key_; }
    bool isWriteCommand() const override { return true; }

private:
    std::string key_;
    std::vector<std::pair<std::string, std::string>> fieldValues_;
};

class HGetCommand : public Command {
public:
    HGetCommand(std::string key, std::string field)
        : key_(std::move(key)), field_(std::move(field)) {}

    protocol::RespValue execute(store::Shard& shard) override;
    std::string getKey() const override { return key_; }
    bool isWriteCommand() const override { return false; }

private:
    std::string key_;
    std::string field_;
};

class HDelCommand : public Command {
public:
    HDelCommand(std::string key, std::vector<std::string> fields)
        : key_(std::move(key)), fields_(std::move(fields)) {}

    protocol::RespValue execute(store::Shard& shard) override;
    std::string getKey() const override { return key_; }
    bool isWriteCommand() const override { return true; }

private:
    std::string key_;
    std::vector<std::string> fields_;
};

class HGetAllCommand : public Command {
public:
    explicit HGetAllCommand(std::string key) : key_(std::move(key)) {}

    protocol::RespValue execute(store::Shard& shard) override;
    std::string getKey() const override { return key_; }
    bool isWriteCommand() const override { return false; }

private:
    std::string key_;
};

class HExistsCommand : public Command {
public:
    HExistsCommand(std::string key, std::string field)
        : key_(std::move(key)), field_(std::move(field)) {}

    protocol::RespValue execute(store::Shard& shard) override;
    std::string getKey() const override { return key_; }
    bool isWriteCommand() const override { return false; }

private:
    std::string key_;
    std::string field_;
};

} // namespace minicache::command
