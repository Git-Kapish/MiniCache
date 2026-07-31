#include "store/active_sweeper.hpp"
#include <chrono>

namespace minicache::store {

ActiveSweeper::ActiveSweeper(ShardRouter& router, uint32_t intervalMs)
    : router_(router), intervalMs_(intervalMs) {}

ActiveSweeper::~ActiveSweeper() {
    stop();
}

void ActiveSweeper::start() {
    if (running_.exchange(true)) {
        return;
    }
    sweeperThread_ = std::thread(&ActiveSweeper::sweepLoop, this);
}

void ActiveSweeper::stop() {
    if (!running_.exchange(false)) {
        return;
    }
    if (sweeperThread_.joinable()) {
        sweeperThread_.join();
    }
}

void ActiveSweeper::sweepLoop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs_));
        if (!running_) {
            break;
        }

        size_t n = router_.numShards();
        for (size_t i = 0; i < n; ++i) {
            router_.getShardByIndex(i).activeExpireCycle(20);
        }
    }
}

} // namespace minicache::store
