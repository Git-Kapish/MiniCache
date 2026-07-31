#pragma once

#include "net/socket_shim.hpp"
#include "store/shard_router.hpp"
#include <string>
#include <thread>
#include <atomic>
#include <cstdint>

namespace minicache::replication {

class ReplicaClient {
public:
    ReplicaClient(const std::string& masterHost, uint16_t masterPort, store::ShardRouter& router);
    ~ReplicaClient();

    ReplicaClient(const ReplicaClient&) = delete;
    ReplicaClient& operator=(const ReplicaClient&) = delete;

    bool start();
    void stop();

    bool isConnected() const { return connected_; }

private:
    void streamLoop();

    std::string masterHost_;
    uint16_t masterPort_;
    store::ShardRouter& router_;
    net::SocketHandle socket_{net::InvalidSocketHandle};
    std::atomic<bool> running_{false};
    std::atomic<bool> connected_{false};
    std::thread streamThread_;
};

} // namespace minicache::replication
