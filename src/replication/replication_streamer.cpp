#include "replication/replication_streamer.hpp"
#include <algorithm>

namespace minicache::replication {

void ReplicationStreamer::addFollower(net::SocketHandle s) {
    std::lock_guard<std::mutex> lock(mutex_);
    followers_.push_back(s);
}

void ReplicationStreamer::removeFollower(net::SocketHandle s) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::find(followers_.begin(), followers_.end(), s);
    if (it != followers_.end()) {
        followers_.erase(it);
    }
}

void ReplicationStreamer::broadcast(const std::string& rawRespBytes) {
    if (rawRespBytes.empty()) return;

    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<net::SocketHandle> broken;

    for (size_t i = 0; i < followers_.size(); ++i) {
        net::SocketHandle s = followers_[i];
        int res = send(s, rawRespBytes.data(), static_cast<int>(rawRespBytes.size()), 0);
        if (res <= 0) {
            broken.push_back(s);
        }
    }

    for (size_t i = 0; i < broken.size(); ++i) {
        net::SocketHandle s = broken[i];
        auto it = std::find(followers_.begin(), followers_.end(), s);
        if (it != followers_.end()) {
            followers_.erase(it);
        }
        net::SocketShim::closeSocket(s);
    }
}

size_t ReplicationStreamer::followerCount() {
    std::lock_guard<std::mutex> lock(mutex_);
    return followers_.size();
}

} // namespace minicache::replication
