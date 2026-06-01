#pragma once
#include <cstdint>
#include <atomic>
#include <memory>
#include <winsock2.h>
#include "Connection.h"
#include "../query/QueryEngine.h"

class Server {
public:
    explicit Server(uint16_t port);
    ~Server();
    bool init();
    void run();
    void stop();

private:
    uint16_t                     port_;
    SOCKET                       listen_fd_ { INVALID_SOCKET };
    std::atomic<bool>            running_   { false };
    std::shared_ptr<QueryEngine> engine_;
};
