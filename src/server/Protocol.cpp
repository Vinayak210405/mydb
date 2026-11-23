#include "Protocol.h"
#include <cstring>

Message Message::query(const std::string &sql) {
    Message m;
    m.type = MessageType::MSG_QUERY;
    m.payload.assign(sql.begin(), sql.end());
    return m;
}

Message Message::result(const std::string &rows) {
    Message m;
    m.type = MessageType::MSG_RESULT;
    m.payload.assign(rows.begin(), rows.end());
    return m;
}

Message Message::ok(const std::string &msg) {
    Message m;
    m.type = MessageType::MSG_OK;
    m.payload.assign(msg.begin(), msg.end());
    return m;
}

Message Message::error(const std::string &msg) {
    Message m;
    m.type = MessageType::MSG_ERROR;
    m.payload.assign(msg.begin(), msg.end());
    return m;
}

int send_message(SOCKET fd, const Message &msg) {
    char header[5];
    header[0] = static_cast<char>(msg.type);
    uint32_t net_len = htonl(static_cast<uint32_t>(msg.payload.size()));
    std::memcpy(header + 1, &net_len, 4);
    if (net::send_all(fd, header, sizeof header) < 0) return -1;
    if (!msg.payload.empty() &&
        net::send_all(fd, reinterpret_cast<const char*>(msg.payload.data()), msg.payload.size()) < 0)
        return -1;
    return 0;
}

int recv_message(SOCKET fd, Message &out) {
    char header[5];
    if (net::recv_exact(fd, header, sizeof header) < 0) return -1;
    out.type = static_cast<MessageType>(static_cast<uint8_t>(header[0]));
    uint32_t net_len;
    std::memcpy(&net_len, header + 1, 4);
    uint32_t len = ntohl(net_len);
    if (len > MAX_MESSAGE_BYTES) return -1;           // refuse absurd sizes
    out.payload.resize(len);
    if (len > 0 && net::recv_exact(fd, reinterpret_cast<char*>(out.payload.data()), len) < 0)
        return -1;
    return 0;
}
