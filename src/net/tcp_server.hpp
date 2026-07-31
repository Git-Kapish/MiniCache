#pragma once

#include "net/socket_shim.hpp"
#include "command/dispatcher.hpp"
#include <atomic>
#include <thread>
#include <vector>
#include <cstdint>

namespace minicache::net {

class TcpServer {
public:
    TcpServer(uint16_t port, command::CommandDispatcher& dispatcher);
    ~TcpServer();

    bool start();
    void stop();

private:
    void acceptLoop();
    void handleClient(SocketHandle clientSock);

    uint16_t port_;
    command::CommandDispatcher& dispatcher_;
    SocketHandle serverSock_{InvalidSocketHandle};
    std::atomic<bool> running_{false};
    std::thread acceptThread_;
};

} // namespace minicache::net
