#include "net/socket_shim.hpp"
#include <iostream>

namespace minicache::net {

bool SocketShim::init() {
#if defined(_WIN32) || defined(_WIN64)
    WSADATA wsaData;
    int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (res != 0) {
        std::cerr << "WSAStartup failed with error: " << res << std::endl;
        return false;
    }
#endif
    return true;
}

void SocketShim::cleanup() {
#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#endif
}

void SocketShim::closeSocket(SocketHandle s) {
    if (s != InvalidSocketHandle) {
#if defined(_WIN32) || defined(_WIN64)
        closesocket(s);
#else
        close(s);
#endif
    }
}

std::string SocketShim::getLastErrorString() {
#if defined(_WIN32) || defined(_WIN64)
    int err = WSAGetLastError();
    return "WSAError code: " + std::to_string(err);
#else
    return std::string(strerror(errno));
#endif
}

} // namespace minicache::net
