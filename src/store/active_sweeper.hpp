#pragma once

#include "store/shard_router.hpp"
#include <thread>
#include <atomic>

namespace minicache::store {

class ActiveSweeper {
public:
    explicit ActiveSweeper(ShardRouter& router, uint32_t intervalMs = 100);
    ~ActiveSweeper();

    void start();
    void stop();

private:
    void sweepLoop();

    ShardRouter& router_;
    uint32_t intervalMs_;
    std::atomic<bool> running_{false};
    std::thread sweeperThread_;
};

} // namespace minicache::store
