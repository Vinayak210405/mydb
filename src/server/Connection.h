#pragma once
#include <string>
#include <memory>
#include <winsock2.h>
#include "../catalog/Catalog.h"
#include "../query/QueryEngine.h"

class Connection {
public:
    Connection(SOCKET fd, std::shared_ptr<QueryEngine> engine);
    ~Connection();
    void run();

private:
    SOCKET                       fd_;
    std::shared_ptr<QueryEngine> engine_;
};
