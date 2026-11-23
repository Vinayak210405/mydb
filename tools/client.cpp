// mydb terminal client — talks to the server's binary protocol (port 5433).
//
//   mydb_client [host] [port]
//
// Type SQL ending with ';' (it can span several lines). \q quits.
#include "server/Net.h"
#include "server/Protocol.h"
#include <iostream>
#include <string>

int main(int argc, char *argv[]) {
    std::string host = argc > 1 ? argv[1] : "127.0.0.1";
    uint16_t port = static_cast<uint16_t>(argc > 2 ? std::stoi(argv[2]) : 5433);

    net::WinsockSession winsock;
    if (!winsock.ok()) { std::cerr << "WSAStartup failed\n"; return 1; }

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1 ||
        connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "could not connect to mydb at " << host << ":" << port
                  << " — is the server running?\n";
        return 1;
    }
    std::cout << "connected to mydb at " << host << ":" << port
              << ". End statements with ';'. \\q to quit.\n";

    std::string buffer, line;
    while (true) {
        std::cout << (buffer.empty() ? "mydb> " : "  ...> ") << std::flush;
        if (!std::getline(std::cin, line)) break;
        if (buffer.empty() && (line == "\\q" || line == "quit" || line == "exit")) break;
        buffer += line + "\n";
        if (line.find(';') == std::string::npos) continue;

        if (send_message(s, Message::query(buffer)) < 0) { std::cerr << "connection lost\n"; break; }
        buffer.clear();
        Message reply;
        if (recv_message(s, reply) < 0) { std::cerr << "connection lost\n"; break; }
        std::cout << reply.payload_str() << "\n";
    }
    net::close_socket(s);
    return 0;
}
