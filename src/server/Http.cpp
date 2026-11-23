#include "Http.h"
#include <sstream>
#include <cctype>
#include <algorithm>

HttpResponse HttpResponse::json(const std::string &body, int status) {
    HttpResponse r;
    r.status = status;
    r.body = body;
    return r;
}

HttpResponse HttpResponse::text(const std::string &body, int status) {
    HttpResponse r;
    r.status = status;
    r.content_type = "text/plain; charset=utf-8";
    r.body = body;
    return r;
}

std::string HttpResponse::serialize() const {
    std::ostringstream ss;
    ss << "HTTP/1.1 " << status << " " << http::status_text(status) << "\r\n"
       << "Content-Type: " << content_type << "\r\n"
       << "Content-Length: " << body.size() << "\r\n"
       << "Cache-Control: no-store\r\n"
       << "X-Content-Type-Options: nosniff\r\n"
       << "Connection: close\r\n\r\n"
       << body;
    return ss.str();
}

namespace http {

std::string url_decode(const std::string &s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '+') out += ' ';
        else if (s[i] == '%' && i + 2 < s.size() &&
                 std::isxdigit(static_cast<unsigned char>(s[i + 1])) &&
                 std::isxdigit(static_cast<unsigned char>(s[i + 2]))) {
            out += static_cast<char>(std::stoi(s.substr(i + 1, 2), nullptr, 16));
            i += 2;
        } else out += s[i];
    }
    return out;
}

std::optional<HttpRequest> parse_head(const std::string &head) {
    std::istringstream in(head);
    std::string line;
    if (!std::getline(in, line)) return std::nullopt;
    if (!line.empty() && line.back() == '\r') line.pop_back();

    HttpRequest req;
    std::istringstream rl(line);
    std::string target, version;
    if (!(rl >> req.method >> target >> version)) return std::nullopt;
    if (version.rfind("HTTP/1.", 0) != 0 || target.empty() || target[0] != '/') return std::nullopt;

    size_t q = target.find('?');
    req.path = url_decode(target.substr(0, q));
    if (q != std::string::npos) {
        std::string qs = target.substr(q + 1);
        size_t start = 0;
        while (start <= qs.size()) {
            size_t amp = qs.find('&', start);
            std::string pair = qs.substr(start, amp == std::string::npos ? std::string::npos : amp - start);
            if (!pair.empty()) {
                size_t eq = pair.find('=');
                if (eq == std::string::npos) req.query[url_decode(pair)] = "";
                else req.query[url_decode(pair.substr(0, eq))] = url_decode(pair.substr(eq + 1));
            }
            if (amp == std::string::npos) break;
            start = amp + 1;
        }
    }

    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) break;
        size_t colon = line.find(':');
        if (colon == std::string::npos) return std::nullopt;
        std::string name = line.substr(0, colon);
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        std::string value = line.substr(colon + 1);
        size_t a = value.find_first_not_of(" \t");
        value = a == std::string::npos ? "" : value.substr(a);
        req.headers[name] = value;
    }
    return req;
}

std::string status_text(int status) {
    switch (status) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Payload Too Large";
        case 500: return "Internal Server Error";
        default:  return "Unknown";
    }
}

std::string content_type_for(const std::string &path) {
    auto ends = [&](const char *ext) {
        std::string e(ext);
        return path.size() >= e.size() && path.compare(path.size() - e.size(), e.size(), e) == 0;
    };
    if (ends(".html")) return "text/html; charset=utf-8";
    if (ends(".js"))   return "text/javascript; charset=utf-8";
    if (ends(".css"))  return "text/css; charset=utf-8";
    if (ends(".svg"))  return "image/svg+xml";
    if (ends(".png"))  return "image/png";
    if (ends(".ico"))  return "image/x-icon";
    if (ends(".json")) return "application/json; charset=utf-8";
    return "application/octet-stream";
}

}  // namespace http
