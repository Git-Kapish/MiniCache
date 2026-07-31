#include "command/dispatcher.hpp"
#include "command/command_factory.hpp"
#include "command/commands.hpp"
#include "protocol/resp_encoder.hpp"

namespace minicache::command {

protocol::RespValue CommandDispatcher::dispatch(const protocol::RespValue& request, net::SocketHandle clientSock) {
    if (request.type == protocol::RespType::Array) {
        const auto& elems = std::get<std::vector<protocol::RespValue>>(request.data);
        if (!elems.empty() && elems[0].type == protocol::RespType::BulkString) {
            std::string cmdName = std::get<std::string>(elems[0].data);
            if (cmdName == "REPLSTART" || cmdName == "replstart") {
                if (streamer_ && clientSock != net::InvalidSocketHandle) {
                    streamer_->addFollower(clientSock);
                }
                return protocol::RespValue::makeSimpleString("OK");
            }
        }
    }

    std::string errorMsg;
    auto cmd = CommandFactory::createCommand(request, errorMsg);
    if (!cmd) {
        return protocol::RespValue::makeError(errorMsg);
    }

    // Follower Read-Only Enforcement
    if (isReadOnly_ && cmd->isWriteCommand()) {
        return protocol::RespValue::makeError("READONLY You can't write against a read only replica.");
    }

    protocol::RespValue response;

    // Multi-key commands (DEL, EXISTS)
    auto* delCmd = dynamic_cast<DelCommand*>(cmd.get());
    if (delCmd) {
        int64_t count = 0;
        for (const auto& k : delCmd->getKeys()) {
            if (router_.getShard(k).del(k)) {
                count++;
            }
        }
        response = protocol::RespValue::makeInteger(count);
    } else {
        auto* existsCmd = dynamic_cast<ExistsCommand*>(cmd.get());
        if (existsCmd) {
            int64_t count = 0;
            for (const auto& k : existsCmd->getKeys()) {
                if (router_.getShard(k).exists(k)) {
                    count++;
                }
            }
            response = protocol::RespValue::makeInteger(count);
        } else {
            // Single-key command routing
            std::string key = cmd->getKey();
            if (key.empty()) {
                response = cmd->execute(router_.getShardByIndex(0));
            } else {
                response = cmd->execute(router_.getShard(key));
            }
        }
    }

    // AOF & Replication Broadcast (Observer Pattern)
    if (cmd->isWriteCommand() && response.type != protocol::RespType::Error) {
        std::string rawCommandBytes = protocol::RespEncoder::encode(request);
        if (aofWriter_) {
            aofWriter_->append(rawCommandBytes);
        }
        if (streamer_) {
            streamer_->broadcast(rawCommandBytes);
        }
    }

    return response;
}

} // namespace minicache::command
