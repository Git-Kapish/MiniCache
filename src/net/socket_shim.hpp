#pragma once

#if defined(_WIN32) || defined(_WIN64)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <unistd.h>
#endif

#include <string>

namespace minicache::net {

#if defined(_WIN32) || defined(_WIN64)
using SocketHandle = SOCKET;
constexpr SocketHandle InvalidSocketHandle = INVALID_SOCKET;
#else
using SocketHandle = int;
constexpr SocketHandle InvalidSocketHandle = -1;
#endif

class SocketShim {
public:
    static bool init();
    static void cleanup();
    static void closeSocket(SocketHandle s);
    static std::string getLastErrorString();
};

} // namespace minicache::net
