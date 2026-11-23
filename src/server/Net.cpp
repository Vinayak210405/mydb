#include "Net.h"

namespace net {

WinsockSession::WinsockSession() {
    WSADATA wsa;
    ok_ = WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
}

WinsockSession::~WinsockSession() {
    if (ok_) WSACleanup();
}

SOCKET listen_on(const std::string &host, uint16_t port, std::string &error) {
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        error = "socket() failed (" + std::to_string(WSAGetLastError()) + ")";
        return INVALID_SOCKET;
    }
    // On Windows SO_REUSEADDR lets another process steal the port;
    // SO_EXCLUSIVEADDRUSE is the safe choice for a server.
    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        error = "invalid IPv4 address '" + host + "'";
        closesocket(s);
        return INVALID_SOCKET;
    }
    if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        error = "bind() failed on " + host + ":" + std::to_string(port) +
                " (" + std::to_string(WSAGetLastError()) + ") — is the port already in use?";
        closesocket(s);
        return INVALID_SOCKET;
    }
    if (listen(s, SOMAXCONN) == SOCKET_ERROR) {
        error = "listen() failed (" + std::to_string(WSAGetLastError()) + ")";
        closesocket(s);
        return INVALID_SOCKET;
    }
    return s;
}

int send_all(SOCKET s, const char *data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        int chunk = static_cast<int>(len - sent > 1 << 20 ? 1 << 20 : len - sent);
        int n = send(s, data + sent, chunk, 0);
        if (n <= 0) return -1;
        sent += static_cast<size_t>(n);
    }
    return 0;
}

int recv_exact(SOCKET s, char *data, size_t len) {
    size_t got = 0;
    while (got < len) {
        int chunk = static_cast<int>(len - got > 1 << 20 ? 1 << 20 : len - got);
        int n = recv(s, data + got, chunk, 0);
        if (n <= 0) return -1;
        got += static_cast<size_t>(n);
    }
    return 0;
}

void close_socket(SOCKET s) {
    if (s != INVALID_SOCKET) closesocket(s);
}

}  // namespace net
