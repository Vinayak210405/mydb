#include "Server.h"
#include <iostream>
#include <thread>

TcpServer::TcpServer(std::string name, std::string host, uint16_t port, Handler handler)
    : name_(std::move(name)), host_(std::move(host)), port_(port), handler_(std::move(handler)) {}

TcpServer::~TcpServer() { stop(); }

bool TcpServer::init() {
    std::string err;
    listen_fd_ = net::listen_on(host_, port_, err);
    if (listen_fd_ == INVALID_SOCKET) {
        std::cerr << "[" << name_ << "] " << err << "\n";
        return false;
    }
    return true;
}

void TcpServer::run() {
    running_ = true;
    while (running_) {
        sockaddr_in client_addr{};
        int addr_len = sizeof(client_addr);
        SOCKET client = accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);
        if (client == INVALID_SOCKET) {
            if (running_) std::cerr << "[" << name_ << "] accept() failed\n";
            break;
        }
        std::thread([h = handler_, client]() { h(client); }).detach();
    }
}

void TcpServer::stop() {
    running_ = false;
    if (listen_fd_ != INVALID_SOCKET) {
        closesocket(listen_fd_);
        listen_fd_ = INVALID_SOCKET;
    }
}
