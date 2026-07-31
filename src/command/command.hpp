#pragma once

#include "protocol/resp_value.hpp"
#include "store/shard.hpp"
#include <string>

namespace minicache::command {

class Command {
public:
    virtual ~Command() = default;
    virtual protocol::RespValue execute(store::Shard& shard) = 0;
    virtual std::string getKey() const { return ""; }
    virtual bool isWriteCommand() const { return false; }
};

} // namespace minicache::command
