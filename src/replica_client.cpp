#include "replication/replica_client.hpp"
#include "protocol/resp_parser.hpp"
#include "command/dispatcher.hpp"
#include "command/commands.hpp"
#include <iostream>
#include <array>

namespace minicache::replication {

ReplicaClient::ReplicaClient(const std::string& masterHost, uint16_t masterPort, store::ShardRouter& router)
    : masterHost_(masterHost), masterPort_(masterPort), router_(router) {}

ReplicaClient::~ReplicaClient() {
    stop();
}

bool ReplicaClient::start() {
    if (!net::SocketShim::init()) {
        return false;
    }

    socket_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_ == net::InvalidSocketHandle) {
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(masterPort_);
    inet_pton(AF_INET, masterHost_.c_str(), &addr.sin_addr);

    if (connect(socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        std::cerr << "Failed to connect to master at " << masterHost_ << ":" << masterPort_ << std::endl;
        net::SocketShim::closeSocket(socket_);
        socket_ = net::InvalidSocketHandle;
        return false;
    }

    // Send REPLSTART registration request to leader
    std::string regCmd = "*1\r\n$9\r\nREPLSTART\r\n";
    send(socket_, regCmd.data(), static_cast<int>(regCmd.size()), 0);

    connected_ = true;
    running_ = true;
    streamThread_ = std::thread(&ReplicaClient::streamLoop, this);
    std::cout << "Replica connected to master at " << masterHost_ << ":" << masterPort_ << std::endl;
    return true;
}

void ReplicaClient::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    if (socket_ != net::InvalidSocketHandle) {
        net::SocketShim::closeSocket(socket_);
        socket_ = net::InvalidSocketHandle;
    }

    if (streamThread_.joinable()) {
        streamThread_.join();
    }
    connected_ = false;
}

void ReplicaClient::streamLoop() {
    std::string readBuf;
    std::array<char, 4096> chunk;
    command::CommandDispatcher internalDispatcher(router_); // Direct execution on follower

    while (running_) {
        int bytesRead = recv(socket_, chunk.data(), static_cast<int>(chunk.size()), 0);
        if (bytesRead <= 0) {
            break;
        }

        readBuf.append(chunk.data(), static_cast<size_t>(bytesRead));

        while (!readBuf.empty()) {
            protocol::RespValue request;
            size_t bytesConsumed = 0;
            protocol::ParseResult res = protocol::RespParser::parse(readBuf, request, bytesConsumed);

            if (res == protocol::ParseResult::Incomplete) {
                break;
            }

            if (res == protocol::ParseResult::Error) {
                readBuf.clear();
                break;
            }

            // Execute streamed command on follower router
            readBuf.erase(0, bytesConsumed);
            internalDispatcher.dispatch(request);
        }
    }

    connected_ = false;
}

} // namespace minicache::replication
