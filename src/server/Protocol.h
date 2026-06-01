#pragma once
#include <cstdint>
#include <string>
#include <vector>
 
// --------------------------------------------------------------------------
// Wire protocol — every message on the TCP stream is:
//
//   [ 1 byte  : MessageType        ]
//   [ 4 bytes : payload length N   ]  (big-endian uint32)
//   [ N bytes : payload            ]
//
// Client → Server:  MSG_QUERY   payload = raw SQL string
// Server → Client:  MSG_RESULT  payload = newline-separated rows
//                   MSG_OK      payload = empty (for INSERT/UPDATE/DELETE)
//                   MSG_ERROR   payload = error string
// --------------------------------------------------------------------------
 
enum class MessageType : uint8_t {
    MSG_QUERY  = 0x01,
    MSG_RESULT = 0x02,
    MSG_OK     = 0x03,
    MSG_ERROR  = 0x04,
};
 
struct Message {
    MessageType type;
    std::vector<uint8_t> payload;
 
    // Convenience: build a message with a string payload
    static Message query(const std::string &sql);
    static Message result(const std::string &rows);
    static Message ok();
    static Message error(const std::string &msg);
 
    // Payload as string (for text-based payloads)
    std::string payload_str() const {
        return std::string(payload.begin(), payload.end());
    }
};
 
// --------------------------------------------------------------------------
// Framed I/O — call these on a raw socket fd
// Both return -1 on error / disconnect, 0 on success
// --------------------------------------------------------------------------
int send_message(int fd, const Message &msg);
int recv_message(int fd, Message &out);