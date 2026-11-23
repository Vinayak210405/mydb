#include "Json.h"
#include <cmath>
#include <cstdio>

std::string JsonWriter::escape(const std::string &s) {
    std::string out;
    out.reserve(s.size() + 2);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

JsonWriter& JsonWriter::value(double d) {
    comma();
    if (!std::isfinite(d)) { out_ << "null"; return *this; }
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.15g", d);
    out_ << buf;
    return *this;
}

JsonWriter& JsonWriter::value(const Value &v) {
    if (is_null(v))   return null();
    if (is_int(v))    return value(std::get<int64_t>(v));
    if (is_float(v))  return value(std::get<double>(v));
    if (is_string(v)) return value(std::get<std::string>(v));
    return value(std::get<bool>(v));
}
