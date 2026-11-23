#pragma once
// ---------------------------------------------------------------------------
// Winsock in one place. Every networking file includes this header instead of
// <winsock2.h> directly.
//
// <windows.h> (pulled in by winsock2.h) defines macros named DELETE, IN and
// OUT, which would silently rewrite our SQL enums (TokenType::DELETE,
// TokenType::IN, ...). We undefine them right after the include.
// ---------------------------------------------------------------------------
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef DELETE
#undef DELETE
#endif
#ifdef IN
#undef IN
#endif
#ifdef OUT
#undef OUT
#endif

#include <string>
#include <cstdint>

namespace net {

// RAII: WSAStartup on construction, WSACleanup on destruction.
class WinsockSession {
public:
    WinsockSession();
    ~WinsockSession();
    bool ok() const { return ok_; }
private:
    bool ok_ = false;
};

// Create a listening TCP socket on host:port. Returns INVALID_SOCKET and
// fills `error` on failure.
SOCKET listen_on(const std::string &host, uint16_t port, std::string &error);

int  send_all(SOCKET s, const char *data, size_t len);   // 0 on success, -1 on error
int  recv_exact(SOCKET s, char *data, size_t len);       // 0 on success, -1 on error/EOF
void close_socket(SOCKET s);

}  // namespace net
