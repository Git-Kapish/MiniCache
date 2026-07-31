#include "net/tcp_server.hpp"
#include "protocol/resp_parser.hpp"
#include "protocol/resp_encoder.hpp"
#include <iostream>
#include <array>

namespace minicache::net {

TcpServer::TcpServer(uint16_t port, command::CommandDispatcher& dispatcher)
    : port_(port), dispatcher_(dispatcher) {}

TcpServer::~TcpServer() {
    stop();
}

bool TcpServer::start() {
    if (!SocketShim::init()) {
        return false;
    }

    serverSock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSock_ == InvalidSocketHandle) {
        std::cerr << "Failed to create socket: " << SocketShim::getLastErrorString() << std::endl;
        return false;
    }

    int opt = 1;
#if defined(_WIN32) || defined(_WIN64)
    setsockopt(serverSock_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
#else
    setsockopt(serverSock_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(serverSock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        std::cerr << "Failed to bind socket to port " << port_ << ": " << SocketShim::getLastErrorString() << std::endl;
        SocketShim::closeSocket(serverSock_);
        serverSock_ = InvalidSocketHandle;
        return false;
    }

    if (listen(serverSock_, SOMAXCONN) != 0) {
        std::cerr << "Failed to listen on socket: " << SocketShim::getLastErrorString() << std::endl;
        SocketShim::closeSocket(serverSock_);
        serverSock_ = InvalidSocketHandle;
        return false;
    }

    running_ = true;
    acceptThread_ = std::thread(&TcpServer::acceptLoop, this);
    std::cout << "MiniCache server listening on 0.0.0.0:" << port_ << std::endl;
    return true;
}

void TcpServer::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    if (serverSock_ != InvalidSocketHandle) {
        SocketShim::closeSocket(serverSock_);
        serverSock_ = InvalidSocketHandle;
    }

    if (acceptThread_.joinable()) {
        acceptThread_.join();
    }

    SocketShim::cleanup();
}

void TcpServer::acceptLoop() {
    while (running_) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        SocketHandle clientSock = accept(serverSock_, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);

        if (clientSock == InvalidSocketHandle) {
            if (!running_) {
                break;
            }
            std::cerr << "Accept failed: " << SocketShim::getLastErrorString() << std::endl;
            continue;
        }

        std::thread([this, clientSock]() {
            handleClient(clientSock);
        }).detach();
    }
}

void TcpServer::handleClient(SocketHandle clientSock) {
    std::string readBuf;
    std::array<char, 4096> chunk;

    while (running_) {
        int bytesRead = recv(clientSock, chunk.data(), static_cast<int>(chunk.size()), 0);
        if (bytesRead <= 0) {
            break; // Connection closed or error
        }

        readBuf.append(chunk.data(), static_cast<size_t>(bytesRead));

        while (!readBuf.empty()) {
            protocol::RespValue request;
            size_t bytesConsumed = 0;
            protocol::ParseResult res = protocol::RespParser::parse(readBuf, request, bytesConsumed);

            if (res == protocol::ParseResult::Incomplete) {
                break; // Need more data from socket
            }

            if (res == protocol::ParseResult::Error) {
                std::string errResp = protocol::RespEncoder::encode(
                    protocol::RespValue::makeError("ERR Protocol error: invalid request"));
                send(clientSock, errResp.data(), static_cast<int>(errResp.size()), 0);
                readBuf.clear();
                break;
            }

            // Success: dispatch command passing client socket
            readBuf.erase(0, bytesConsumed);
            protocol::RespValue response = dispatcher_.dispatch(request, clientSock);
            std::string encodedResp = protocol::RespEncoder::encode(response);

            send(clientSock, encodedResp.data(), static_cast<int>(encodedResp.size()), 0);
        }
    }

    SocketShim::closeSocket(clientSock);
}

} // namespace minicache::net
