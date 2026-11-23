#pragma once
#include <cstdint>
#include <atomic>
#include <string>
#include <functional>
#include "Net.h"

// ---------------------------------------------------------------------------
// TcpServer — accept loop shared by the binary-protocol server and the HTTP
// server. Each accepted client is handled on its own thread by `handler`,
// which owns (and must close) the socket.
// ---------------------------------------------------------------------------
class TcpServer {
public:
    using Handler = std::function<void(SOCKET)>;

    TcpServer(std::string name, std::string host, uint16_t port, Handler handler);
    ~TcpServer();

    bool init();          // bind + listen; prints the error on failure
    void run();           // blocks until stop()
    void stop();

private:
    std::string       name_, host_;
    uint16_t          port_;
    Handler           handler_;
    SOCKET            listen_fd_ { INVALID_SOCKET };
    std::atomic<bool> running_   { false };
};
