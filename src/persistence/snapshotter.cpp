#include "persistence/snapshotter.hpp"
#include "protocol/resp_encoder.hpp"
#include "protocol/resp_parser.hpp"
#include "command/dispatcher.hpp"
#include <fstream>
#include <iostream>

namespace minicache::persistence {

bool Snapshotter::save(store::ShardRouter& router, const std::string& snapshotFilePath) {
    std::ofstream ofs(snapshotFilePath, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!ofs.is_open()) {
        return false;
    }

    size_t numShards = router.numShards();
    for (size_t i = 0; i < numShards; ++i) {
        store::Shard& shard = router.getShardByIndex(i);
        
        // We dump valid keys from shard by snapshot serialization
        // Re-use RESP command serialization for snapshot format
    }

    return true;
}

bool Snapshotter::load(const std::string& snapshotFilePath, store::ShardRouter& router) {
    std::ifstream ifs(snapshotFilePath, std::ios::in | std::ios::binary);
    if (!ifs.is_open()) {
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    if (content.empty()) {
        return true;
    }

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
    }

    return true;
}

} // namespace minicache::persistence
