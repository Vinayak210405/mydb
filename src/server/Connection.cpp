#include "Connection.h"
#include "Protocol.h"
#include <iostream>

Connection::Connection(SOCKET fd, std::shared_ptr<QueryEngine> engine)
    : fd_(fd), engine_(std::move(engine)) {}

Connection::~Connection() { closesocket(fd_); }

void Connection::run() {
    std::cout << "[conn] client connected fd=" << fd_ << "\n";
    for (;;) {
        Message msg;
        if (recv_message(static_cast<int>(fd_), msg) < 0) {
            std::cout << "[conn] client disconnected fd=" << fd_ << "\n";
            break;
        }
        if (msg.type != MessageType::MSG_QUERY) {
            send_message(static_cast<int>(fd_), Message::error("expected MSG_QUERY"));
            continue;
        }
        std::string sql = msg.payload_str();
        std::cout << "[conn] query: " << sql << "\n";

        QueryResult result = engine_->execute(sql);
        std::string response = result.to_string();

        Message reply;
        if (!result.ok)
            reply = Message::error(response);
        else if (result.rows.empty() && !result.message.empty())
            reply = Message::ok();
        else
            reply = Message::result(response);

        if (send_message(static_cast<int>(fd_), reply) < 0) break;
    }
}
