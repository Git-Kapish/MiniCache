#pragma once

#include "net/socket_shim.hpp"
#include <string>
#include <vector>
#include <mutex>

namespace minicache::replication {

class ReplicationStreamer {
public:
    ReplicationStreamer() = default;
    ~ReplicationStreamer() = default;

    ReplicationStreamer(const ReplicationStreamer&) = delete;
    ReplicationStreamer& operator=(const ReplicationStreamer&) = delete;

    void addFollower(net::SocketHandle s);
    void removeFollower(net::SocketHandle s);
    void broadcast(const std::string& rawRespBytes);

    size_t followerCount();

private:
    std::vector<net::SocketHandle> followers_;
    std::mutex mutex_;
};

} // namespace minicache::replication
