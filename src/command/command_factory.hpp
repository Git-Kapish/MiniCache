#pragma once

#include "command/command.hpp"
#include "protocol/resp_value.hpp"
#include <memory>
#include <string>

namespace minicache::command {

class CommandFactory {
public:
    static std::unique_ptr<Command> createCommand(const protocol::RespValue& value, std::string& errorMsg);
};

} // namespace minicache::command
