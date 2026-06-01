#include "Server.h"
#include <iostream>
#include <thread>
#include <ws2tcpip.h>

Server::Server(uint16_t port) : port_(port) {}

Server::~Server() {
    stop();
    if (listen_fd_ != INVALID_SOCKET) {
        closesocket(listen_fd_);
        listen_fd_ = INVALID_SOCKET;
    }
    WSACleanup();
}

bool Server::init() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "[server] WSAStartup failed\n";
        return false;
    }
    listen_fd_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_fd_ == INVALID_SOCKET) {
        std::cerr << "[server] socket() failed\n";
        return false;
    }
    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port_);
    if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "[server] bind() failed on port " << port_ << "\n";
        return false;
    }
    if (listen(listen_fd_, 128) == SOCKET_ERROR) {
        std::cerr << "[server] listen() failed\n";
        return false;
    }
    std::cout << "[server] listening on port " << port_ << "\n";
    return true;
}

void Server::run() {
    running_ = true;
    while (running_) {
        sockaddr_in client_addr{};
        int addr_len = sizeof(client_addr);
        SOCKET client_fd = accept(listen_fd_,
                                  reinterpret_cast<sockaddr*>(&client_addr),
                                  &addr_len);
        if (client_fd == INVALID_SOCKET) {
            if (running_) std::cerr << "[server] accept() failed\n";
            break;
        }
        std::thread([client_fd]() {
            Connection conn(client_fd);
            conn.run();
        }).detach();
    }
}

void Server::stop() {
    running_ = false;
    if (listen_fd_ != INVALID_SOCKET) {
        closesocket(listen_fd_);
        listen_fd_ = INVALID_SOCKET;
    }
}
