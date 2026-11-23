#pragma once
#include <string>
#include <map>
#include <optional>

// Plain-data HTTP/1.1 request and response, plus the parsing/serialising
// code. No sockets here, so it is unit-testable; HttpServer does the I/O.
struct HttpRequest {
    std::string                        method;
    std::string                        path;     // decoded, without the query string
    std::map<std::string, std::string> query;
    std::map<std::string, std::string> headers;  // lower-case names
    std::string                        body;

    std::string param(const std::string &name, const std::string &def = "") const {
        auto it = query.find(name);
        return it == query.end() ? def : it->second;
    }
};

struct HttpResponse {
    int         status = 200;
    std::string content_type = "application/json; charset=utf-8";
    std::string body;

    static HttpResponse json(const std::string &body, int status = 200);
    static HttpResponse text(const std::string &body, int status);
    std::string serialize() const;
};

namespace http {

constexpr size_t MAX_HEADER_BYTES = 16 * 1024;
constexpr size_t MAX_BODY_BYTES   = 8 * 1024 * 1024;

// Parse the request line + headers (everything before the blank line).
// Returns std::nullopt if malformed.
std::optional<HttpRequest> parse_head(const std::string &head);

std::string url_decode(const std::string &s);
std::string status_text(int status);
std::string content_type_for(const std::string &path);

}  // namespace http
