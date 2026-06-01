#include "Connection.h"
#include "Protocol.h"
#include <iostream>
#include <algorithm>
#include <cctype>
 
Connection::Connection(SOCKET fd) : fd_(fd) {}
 
Connection::~Connection() {
    closesocket(fd_);
}
 
std::string Connection::dispatch(const std::string &sql) {
    // Trim and uppercase for comparison
    std::string s = sql;
    s.erase(0, s.find_first_not_of(" \t\r\n"));
    std::string upper = s;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
 
    // Stub responses — swap these out when QueryEngine exists
    if (upper.find("SELECT 1") != std::string::npos)
        return "1";
 
    if (upper.find("SELECT") != std::string::npos)
        return "ERROR: tables not implemented yet";
 
    if (upper.find("INSERT") != std::string::npos)
        return "OK: 1 row inserted (stub)";
 
    if (upper.find("CREATE TABLE") != std::string::npos)
        return "OK: table created (stub)";
 
    return "ERROR: unknown statement";
}
 
void Connection::run() {
    std::cout << "[conn] client connected (fd=" << fd_ << ")\n";
 
    for (;;) {
        Message msg;
        if (recv_message(static_cast<int>(fd_), msg) < 0) {
            std::cout << "[conn] client disconnected (fd=" << fd_ << ")\n";
            break;
        }
 
        if (msg.type != MessageType::MSG_QUERY) {
            auto err = Message::error("expected MSG_QUERY");
            send_message(static_cast<int>(fd_), err);
            continue;
        }
 
        std::string sql = msg.payload_str();
        std::cout << "[conn] query: " << sql << "\n";
 
        std::string response = dispatch(sql);
 
        // Decide response type
        Message reply;
        if (response.rfind("ERROR:", 0) == 0)
            reply = Message::error(response);
        else if (response.rfind("OK:", 0) == 0)
            reply = Message::ok();
        else
            reply = Message::result(response);
 
        if (send_message(static_cast<int>(fd_), reply) < 0) {
            std::cout << "[conn] send failed (fd=" << fd_ << ")\n";
            break;
        }
    }
}