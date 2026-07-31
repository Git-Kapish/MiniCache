#pragma once

#include "store/shard_router.hpp"
#include <string>
#include <cstddef>

namespace minicache::persistence {

class Recovery {
public:
    static size_t recover(store::ShardRouter& router, const std::string& snapshotFilePath, const std::string& aofFilePath);
};

} // namespace minicache::persistence
