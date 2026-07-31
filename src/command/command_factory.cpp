#include "command/command_factory.hpp"
#include "command/commands.hpp"
#include "command/hash_commands.hpp"
#include "command/list_commands.hpp"
#include "command/set_commands.hpp"
#include <algorithm>
#include <cctype>

namespace minicache::command {

namespace {

std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return s;
}

} // namespace

std::unique_ptr<Command> CommandFactory::createCommand(const protocol::RespValue& value, std::string& errorMsg) {
    if (value.type != protocol::RespType::Array) {
        errorMsg = "ERR invalid command format, expected Array";
        return nullptr;
    }

    const auto& args = std::get<std::vector<protocol::RespValue>>(value.data);
    if (args.empty()) {
        errorMsg = "ERR empty command";
        return nullptr;
    }

    std::vector<std::string> strArgs;
    strArgs.reserve(args.size());

    for (const auto& arg : args) {
        if (arg.type == protocol::RespType::BulkString || arg.type == protocol::RespType::SimpleString) {
            strArgs.push_back(std::get<std::string>(arg.data));
        } else if (arg.type == protocol::RespType::Integer) {
            strArgs.push_back(std::to_string(std::get<int64_t>(arg.data)));
        } else {
            errorMsg = "ERR command arguments must be strings";
            return nullptr;
        }
    }

    std::string cmdName = toUpper(strArgs[0]);

    // Core Key-Value
    if (cmdName == "SET") {
        if (strArgs.size() < 3) {
            errorMsg = "ERR wrong number of arguments for 'set' command";
            return nullptr;
        }
        std::string key = strArgs[1];
        std::string val = strArgs[2];
        std::optional<uint64_t> ttlMs = std::nullopt;

        if (strArgs.size() >= 5) {
            std::string opt = toUpper(strArgs[3]);
            try {
                uint64_t t = std::stoull(strArgs[4]);
                if (opt == "EX") {
                    ttlMs = t * 1000;
                } else if (opt == "PX") {
                    ttlMs = t;
                }
            } catch (...) {
                errorMsg = "ERR value is not an integer or out of range";
                return nullptr;
            }
        }
        return std::make_unique<SetCommand>(std::move(key), std::move(val), ttlMs);
    }

    if (cmdName == "GET") {
        if (strArgs.size() != 2) {
            errorMsg = "ERR wrong number of arguments for 'get' command";
            return nullptr;
        }
        return std::make_unique<GetCommand>(std::move(strArgs[1]));
    }

    if (cmdName == "DEL") {
        if (strArgs.size() < 2) {
            errorMsg = "ERR wrong number of arguments for 'del' command";
            return nullptr;
        }
        std::vector<std::string> keys(strArgs.begin() + 1, strArgs.end());
        return std::make_unique<DelCommand>(std::move(keys));
    }

    if (cmdName == "EXISTS") {
        if (strArgs.size() < 2) {
            errorMsg = "ERR wrong number of arguments for 'exists' command";
            return nullptr;
        }
        std::vector<std::string> keys(strArgs.begin() + 1, strArgs.end());
        return std::make_unique<ExistsCommand>(std::move(keys));
    }

    if (cmdName == "EXPIRE") {
        if (strArgs.size() != 3) {
            errorMsg = "ERR wrong number of arguments for 'expire' command";
            return nullptr;
        }
        try {
            uint64_t sec = std::stoull(strArgs[2]);
            return std::make_unique<ExpireCommand>(std::move(strArgs[1]), sec);
        } catch (...) {
            errorMsg = "ERR value is not an integer or out of range";
            return nullptr;
        }
    }

    if (cmdName == "TTL") {
        if (strArgs.size() != 2) {
            errorMsg = "ERR wrong number of arguments for 'ttl' command";
            return nullptr;
        }
        return std::make_unique<TtlCommand>(std::move(strArgs[1]));
    }

    if (cmdName == "PERSIST") {
        if (strArgs.size() != 2) {
            errorMsg = "ERR wrong number of arguments for 'persist' command";
            return nullptr;
        }
        return std::make_unique<PersistCommand>(std::move(strArgs[1]));
    }

    if (cmdName == "INCR") {
        if (strArgs.size() != 2) {
            errorMsg = "ERR wrong number of arguments for 'incr' command";
            return nullptr;
        }
        return std::make_unique<IncrByCommand>(std::move(strArgs[1]), 1);
    }

    if (cmdName == "DECR") {
        if (strArgs.size() != 2) {
            errorMsg = "ERR wrong number of arguments for 'decr' command";
            return nullptr;
        }
        return std::make_unique<IncrByCommand>(std::move(strArgs[1]), -1);
    }

    if (cmdName == "INCRBY") {
        if (strArgs.size() != 3) {
            errorMsg = "ERR wrong number of arguments for 'incrby' command";
            return nullptr;
        }
        try {
            int64_t delta = std::stoll(strArgs[2]);
            return std::make_unique<IncrByCommand>(std::move(strArgs[1]), delta);
        } catch (...) {
            errorMsg = "ERR value is not an integer or out of range";
            return nullptr;
        }
    }

    if (cmdName == "DECRBY") {
        if (strArgs.size() != 3) {
            errorMsg = "ERR wrong number of arguments for 'decrby' command";
            return nullptr;
        }
        try {
            int64_t delta = std::stoll(strArgs[2]);
            return std::make_unique<IncrByCommand>(std::move(strArgs[1]), -delta);
        } catch (...) {
            errorMsg = "ERR value is not an integer or out of range";
            return nullptr;
        }
    }

    if (cmdName == "PING") {
        std::string msg = (strArgs.size() > 1) ? strArgs[1] : "";
        return std::make_unique<PingCommand>(std::move(msg));
    }

    // Hash Commands
    if (cmdName == "HSET") {
        if (strArgs.size() < 4 || (strArgs.size() % 2 != 0)) {
            errorMsg = "ERR wrong number of arguments for 'hset' command";
            return nullptr;
        }
        std::string key = strArgs[1];
        std::vector<std::pair<std::string, std::string>> fieldValues;
        for (size_t i = 2; i < strArgs.size(); i += 2) {
            fieldValues.emplace_back(strArgs[i], strArgs[i + 1]);
        }
        return std::make_unique<HSetCommand>(std::move(key), std::move(fieldValues));
    }

    if (cmdName == "HGET") {
        if (strArgs.size() != 3) {
            errorMsg = "ERR wrong number of arguments for 'hget' command";
            return nullptr;
        }
        return std::make_unique<HGetCommand>(std::move(strArgs[1]), std::move(strArgs[2]));
    }

    if (cmdName == "HDEL") {
        if (strArgs.size() < 3) {
            errorMsg = "ERR wrong number of arguments for 'hdel' command";
            return nullptr;
        }
        std::vector<std::string> fields(strArgs.begin() + 2, strArgs.end());
        return std::make_unique<HDelCommand>(std::move(strArgs[1]), std::move(fields));
    }

    if (cmdName == "HGETALL") {
        if (strArgs.size() != 2) {
            errorMsg = "ERR wrong number of arguments for 'hgetall' command";
            return nullptr;
        }
        return std::make_unique<HGetAllCommand>(std::move(strArgs[1]));
    }

    if (cmdName == "HEXISTS") {
        if (strArgs.size() != 3) {
            errorMsg = "ERR wrong number of arguments for 'hexists' command";
            return nullptr;
        }
        return std::make_unique<HExistsCommand>(std::move(strArgs[1]), std::move(strArgs[2]));
    }

    // List Commands
    if (cmdName == "LPUSH") {
        if (strArgs.size() < 3) {
            errorMsg = "ERR wrong number of arguments for 'lpush' command";
            return nullptr;
        }
        std::vector<std::string> vals(strArgs.begin() + 2, strArgs.end());
        return std::make_unique<LPushCommand>(std::move(strArgs[1]), std::move(vals));
    }

    if (cmdName == "RPUSH") {
        if (strArgs.size() < 3) {
            errorMsg = "ERR wrong number of arguments for 'rpush' command";
            return nullptr;
        }
        std::vector<std::string> vals(strArgs.begin() + 2, strArgs.end());
        return std::make_unique<RPushCommand>(std::move(strArgs[1]), std::move(vals));
    }

    if (cmdName == "LPOP") {
        if (strArgs.size() != 2) {
            errorMsg = "ERR wrong number of arguments for 'lpop' command";
            return nullptr;
        }
        return std::make_unique<LPopCommand>(std::move(strArgs[1]));
    }

    if (cmdName == "RPOP") {
        if (strArgs.size() != 2) {
            errorMsg = "ERR wrong number of arguments for 'rpop' command";
            return nullptr;
        }
        return std::make_unique<RPopCommand>(std::move(strArgs[1]));
    }

    if (cmdName == "LRANGE") {
        if (strArgs.size() != 4) {
            errorMsg = "ERR wrong number of arguments for 'lrange' command";
            return nullptr;
        }
        try {
            int64_t start = std::stoll(strArgs[2]);
            int64_t stop = std::stoll(strArgs[3]);
            return std::make_unique<LRangeCommand>(std::move(strArgs[1]), start, stop);
        } catch (...) {
            errorMsg = "ERR value is not an integer or out of range";
            return nullptr;
        }
    }

    if (cmdName == "LLEN") {
        if (strArgs.size() != 2) {
            errorMsg = "ERR wrong number of arguments for 'llen' command";
            return nullptr;
        }
        return std::make_unique<LLenCommand>(std::move(strArgs[1]));
    }

    // Set Commands
    if (cmdName == "SADD") {
        if (strArgs.size() < 3) {
            errorMsg = "ERR wrong number of arguments for 'sadd' command";
            return nullptr;
        }
        std::vector<std::string> members(strArgs.begin() + 2, strArgs.end());
        return std::make_unique<SAddCommand>(std::move(strArgs[1]), std::move(members));
    }

    if (cmdName == "SREM") {
        if (strArgs.size() < 3) {
            errorMsg = "ERR wrong number of arguments for 'srem' command";
            return nullptr;
        }
        std::vector<std::string> members(strArgs.begin() + 2, strArgs.end());
        return std::make_unique<SRemCommand>(std::move(strArgs[1]), std::move(members));
    }

    if (cmdName == "SISMEMBER") {
        if (strArgs.size() != 3) {
            errorMsg = "ERR wrong number of arguments for 'sismember' command";
            return nullptr;
        }
        return std::make_unique<SIsMemberCommand>(std::move(strArgs[1]), std::move(strArgs[2]));
    }

    if (cmdName == "SMEMBERS") {
        if (strArgs.size() != 2) {
            errorMsg = "ERR wrong number of arguments for 'smembers' command";
            return nullptr;
        }
        return std::make_unique<SMembersCommand>(std::move(strArgs[1]));
    }

    errorMsg = "ERR unknown command '" + strArgs[0] + "'";
    return nullptr;
}

} // namespace minicache::command
