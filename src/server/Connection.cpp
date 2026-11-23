#include "Connection.h"
#include "Protocol.h"
#include "Api.h"
#include "Http.h"
#include <iostream>
#include <cstdlib>

void handle_binary_client(SOCKET fd, QueryEngine &engine) {
    for (;;) {
        Message msg;
        if (recv_message(fd, msg) < 0) break;
        if (msg.type != MessageType::MSG_QUERY) {
            if (send_message(fd, Message::error("expected MSG_QUERY")) < 0) break;
            continue;
        }
        auto results = engine.execute_script(msg.payload_str());
        std::string out;
        bool ok = true;
        for (auto &r : results) {
            if (!out.empty()) out += "\n\n";
            out += r.to_string();
            ok = ok && r.ok;
        }
        Message reply;
        if (!ok)                                       reply = Message::error(out);
        else if (results.size() == 1 && results[0].columns.empty()) reply = Message::ok(out);
        else                                           reply = Message::result(out);
        if (send_message(fd, reply) < 0) break;
    }
    net::close_socket(fd);
}

void handle_http_client(SOCKET fd, ApiHandler &api) {
    // Read until the blank line that ends the headers.
    std::string data;
    char buf[8192];
    size_t head_end = std::string::npos;
    while (head_end == std::string::npos) {
        int n = recv(fd, buf, sizeof buf, 0);
        if (n <= 0) { net::close_socket(fd); return; }
        data.append(buf, static_cast<size_t>(n));
        head_end = data.find("\r\n\r\n");
        if (head_end == std::string::npos && data.size() > http::MAX_HEADER_BYTES) {
            std::string r = HttpResponse::text("headers too large", 400).serialize();
            net::send_all(fd, r.data(), r.size());
            net::close_socket(fd);
            return;
        }
    }

    HttpResponse response;
    auto req = http::parse_head(data.substr(0, head_end + 2));
    if (!req) {
        response = HttpResponse::text("bad request", 400);
    } else {
        size_t length = 0;
        auto it = req->headers.find("content-length");
        if (it != req->headers.end()) length = std::strtoull(it->second.c_str(), nullptr, 10);
        if (length > http::MAX_BODY_BYTES) {
            response = HttpResponse::text("request body too large", 413);
        } else {
            req->body = data.substr(head_end + 4);
            if (req->body.size() < length) {
                size_t have = req->body.size();
                req->body.resize(length);
                if (net::recv_exact(fd, &req->body[have], length - have) < 0) {
                    net::close_socket(fd);
                    return;
                }
            }
            req->body.resize(length);
            response = api.handle(*req);
            if (req->method == "HEAD") response.body.clear();
        }
    }
    std::string out = response.serialize();
    net::send_all(fd, out.data(), out.size());
    net::close_socket(fd);
}
