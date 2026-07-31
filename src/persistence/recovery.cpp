#include "persistence/recovery.hpp"
#include "persistence/snapshotter.hpp"
#include "protocol/resp_parser.hpp"
#include "command/dispatcher.hpp"
#include <fstream>
#include <iostream>

namespace minicache::persistence {

size_t Recovery::recover(store::ShardRouter& router, const std::string& snapshotFilePath, const std::string& aofFilePath) {
    size_t replayedCommands = 0;

    // 1. Load snapshot if present
    if (!snapshotFilePath.empty()) {
        Snapshotter::load(snapshotFilePath, router);
    }

    // 2. Replay AOF file
    if (!aofFilePath.empty()) {
        std::ifstream ifs(aofFilePath, std::ios::in | std::ios::binary);
        if (ifs.is_open()) {
            std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
            if (!content.empty()) {
                command::CommandDispatcher dispatcher(router);
                size_t offset = 0;

                while (offset < content.size()) {
                    protocol::RespValue cmdVal;
                    size_t consumed = 0;
                    protocol::ParseResult res = protocol::RespParser::parse(std::string_view(content).substr(offset), cmdVal, consumed);
                    if (res != protocol::ParseResult::Success) {
                        break;
                    }
                    dispatcher.dispatch(cmdVal);
                    offset += consumed;
                    replayedCommands++;
                }
            }
        }
    }

    return replayedCommands;
}

} // namespace minicache::persistence
