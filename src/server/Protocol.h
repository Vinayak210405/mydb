#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "Net.h"

// --------------------------------------------------------------------------
// Binary wire protocol (port 5433, used by tools/client.cpp). Every message is:
//
//   [ 1 byte  : MessageType        ]
//   [ 4 bytes : payload length N   ]  (big-endian uint32)
//   [ N bytes : payload            ]
//
// Client → Server:  MSG_QUERY   payload = SQL text
// Server → Client:  MSG_RESULT  payload = formatted result table
//                   MSG_OK      payload = status message ("3 rows inserted")
//                   MSG_ERROR   payload = error text
// --------------------------------------------------------------------------
enum class MessageType : uint8_t {
    MSG_QUERY  = 0x01,
    MSG_RESULT = 0x02,
    MSG_OK     = 0x03,
    MSG_ERROR  = 0x04,
};

constexpr uint32_t MAX_MESSAGE_BYTES = 16 * 1024 * 1024;

struct Message {
    MessageType          type = MessageType::MSG_OK;
    std::vector<uint8_t> payload;

    static Message query(const std::string &sql);
    static Message result(const std::string &rows);
    static Message ok(const std::string &msg = "");
    static Message error(const std::string &msg);

    std::string payload_str() const { return std::string(payload.begin(), payload.end()); }
};

// Both return -1 on error / disconnect, 0 on success.
int send_message(SOCKET fd, const Message &msg);
int recv_message(SOCKET fd, Message &out);
