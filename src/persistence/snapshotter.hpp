#pragma once

#include "store/shard_router.hpp"
#include <string>

namespace minicache::persistence {

class Snapshotter {
public:
    static bool save(store::ShardRouter& router, const std::string& snapshotFilePath);
    static bool load(const std::string& snapshotFilePath, store::ShardRouter& router);
};

} // namespace minicache::persistence
