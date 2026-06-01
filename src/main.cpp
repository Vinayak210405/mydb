#include "server/Server.h"
#include <iostream>
#include <csignal>

static Server *g_server = nullptr;

static void handle_signal(int) {
    std::cout << "\n[main] shutting down...\n";
    if (g_server) g_server->stop();
}

int main(int argc, char *argv[]) {
    uint16_t port = 5433;
    if (argc == 2) port = static_cast<uint16_t>(std::stoi(argv[1]));

    std::signal(SIGINT,  handle_signal);
    std::signal(SIGTERM, handle_signal);

    Server server(port);
    g_server = &server;

    if (!server.init()) {
        std::cerr << "[main] failed to start server\n";
        return 1;
    }

    server.run();
    std::cout << "[main] bye\n";
    return 0;
}
