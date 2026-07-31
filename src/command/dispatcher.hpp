#pragma once

#include "protocol/resp_value.hpp"
#include "store/shard_router.hpp"
#include "persistence/aof_writer.hpp"
#include "replication/replication_streamer.hpp"

namespace minicache::command {

class CommandDispatcher {
public:
    explicit CommandDispatcher(
        store::ShardRouter& router,
        persistence::AofWriter* aofWriter = nullptr,
        replication::ReplicationStreamer* streamer = nullptr,
        bool isReadOnly = false
    ) : router_(router), aofWriter_(aofWriter), streamer_(streamer), isReadOnly_(isReadOnly) {}

    protocol::RespValue dispatch(const protocol::RespValue& request, net::SocketHandle clientSock = net::InvalidSocketHandle);

    void setReadOnly(bool readOnly) { isReadOnly_ = readOnly; }
    bool isReadOnly() const { return isReadOnly_; }

private:
    store::ShardRouter& router_;
    persistence::AofWriter* aofWriter_;
    replication::ReplicationStreamer* streamer_;
    bool isReadOnly_{false};
};

} // namespace minicache::command
