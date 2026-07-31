#pragma once

#include "command/command.hpp"
#include <string>
#include <vector>

namespace minicache::command {

class SAddCommand : public Command {
public:
    SAddCommand(std::string key, std::vector<std::string> members)
        : key_(std::move(key)), members_(std::move(members)) {}

    protocol::RespValue execute(store::Shard& shard) override;
    std::string getKey() const override { return key_; }
    bool isWriteCommand() const override { return true; }

private:
    std::string key_;
    std::vector<std::string> members_;
};

class SRemCommand : public Command {
public:
    SRemCommand(std::string key, std::vector<std::string> members)
        : key_(std::move(key)), members_(std::move(members)) {}

    protocol::RespValue execute(store::Shard& shard) override;
    std::string getKey() const override { return key_; }
    bool isWriteCommand() const override { return true; }

private:
    std::string key_;
    std::vector<std::string> members_;
};

class SIsMemberCommand : public Command {
public:
    SIsMemberCommand(std::string key, std::string member)
        : key_(std::move(key)), member_(std::move(member)) {}

    protocol::RespValue execute(store::Shard& shard) override;
    std::string getKey() const override { return key_; }
    bool isWriteCommand() const override { return false; }

private:
    std::string key_;
    std::string member_;
};

class SMembersCommand : public Command {
public:
    explicit SMembersCommand(std::string key) : key_(std::move(key)) {}

    protocol::RespValue execute(store::Shard& shard) override;
    std::string getKey() const override { return key_; }
    bool isWriteCommand() const override { return false; }

private:
    std::string key_;
};

} // namespace minicache::command
