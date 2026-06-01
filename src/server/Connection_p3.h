#pragma once
#include <string>
#include <winsock2.h>
 
// --------------------------------------------------------------------------
// Connection owns one accepted client socket.
// Created by Server, runs its loop in a dedicated thread.
// --------------------------------------------------------------------------
class Connection {
public:
    explicit Connection(SOCKET fd);
    ~Connection();
 
    // Blocks until client disconnects. Called from the thread entry.
    void run();
 
private:
    SOCKET fd_;
 
    // Stub dispatcher — replace with real QueryEngine call later
    std::string dispatch(const std::string &sql);
};