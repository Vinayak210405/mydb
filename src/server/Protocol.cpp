#include "Protocol.h"
 
#include <cstring>
 
// Windows socket headers
#include <winsock2.h>
#include <ws2tcpip.h>
 
// --------------------------------------------------------------------------
// Low-level: read/write exact byte counts
// --------------------------------------------------------------------------
static int read_exact(SOCKET fd, void *buf, size_t n) {
    size_t total = 0;
    char *ptr = reinterpret_cast<char *>(buf);
    while (total < n) {
        int r = recv(fd, ptr + total, static_cast<int>(n - total), 0);
        if (r <= 0) return -1;   // disconnect or error
        total += static_cast<size_t>(r);
    }
    return 0;
}
 
static int write_exact(SOCKET fd, const void *buf, size_t n) {
    size_t total = 0;
    const char *ptr = reinterpret_cast<const char *>(buf);
    while (total < n) {
        int w = send(fd, ptr + total, static_cast<int>(n - total), 0);
        if (w <= 0) return -1;
        total += static_cast<size_t>(w);
    }
    return 0;
}
 
// --------------------------------------------------------------------------
// Message factories
// --------------------------------------------------------------------------
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
 
Message Message::ok() {
    Message m;
    m.type = MessageType::MSG_OK;
    return m;
}
 
Message Message::error(const std::string &msg) {
    Message m;
    m.type = MessageType::MSG_ERROR;
    m.payload.assign(msg.begin(), msg.end());
    return m;
}
 
// --------------------------------------------------------------------------
// Send:  [1 byte type] [4 bytes length BE] [payload]
// --------------------------------------------------------------------------
int send_message(int fd, const Message &msg) {
    SOCKET sock = static_cast<SOCKET>(fd);
 
    uint8_t type_byte = static_cast<uint8_t>(msg.type);
    if (write_exact(sock, &type_byte, 1) < 0) return -1;
 
    uint32_t net_len = htonl(static_cast<uint32_t>(msg.payload.size()));
    if (write_exact(sock, &net_len, 4) < 0) return -1;
 
    if (!msg.payload.empty()) {
        if (write_exact(sock, msg.payload.data(), msg.payload.size()) < 0)
            return -1;
    }
    return 0;
}
 
// --------------------------------------------------------------------------
// Recv:  read type, then length, then payload
// --------------------------------------------------------------------------
int recv_message(int fd, Message &out) {
    SOCKET sock = static_cast<SOCKET>(fd);
 
    uint8_t type_byte;
    if (read_exact(sock, &type_byte, 1) < 0) return -1;
    out.type = static_cast<MessageType>(type_byte);
 
    uint32_t net_len;
    if (read_exact(sock, &net_len, 4) < 0) return -1;
    uint32_t len = ntohl(net_len);
 
    out.payload.resize(len);
    if (len > 0) {
        if (read_exact(sock, out.payload.data(), len) < 0) return -1;
    }
    return 0;
}