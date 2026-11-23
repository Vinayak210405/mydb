#include "Tuple.h"
#include <cstring>
#include <stdexcept>

namespace {
template <typename T>
void put(std::vector<char> &buf, const T &v) {
    size_t at = buf.size();
    buf.resize(at + sizeof(T));
    std::memcpy(buf.data() + at, &v, sizeof(T));
}

void need(size_t pos, size_t n, size_t len) {
    if (pos + n > len) throw std::runtime_error("Tuple: corrupt row (truncated)");
}
}  // namespace

std::vector<char> Tuple::serialize(const std::vector<Value> &values) {
    std::vector<char> buf;
    for (auto &v : values) {
        if (is_null(v)) {
            buf.push_back(0);
        } else if (is_int(v)) {
            buf.push_back(1);
            put(buf, std::get<int64_t>(v));
        } else if (is_float(v)) {
            buf.push_back(2);
            put(buf, std::get<double>(v));
        } else if (is_string(v)) {
            buf.push_back(3);
            const std::string &s = std::get<std::string>(v);
            put(buf, static_cast<uint32_t>(s.size()));
            buf.insert(buf.end(), s.begin(), s.end());
        } else {
            buf.push_back(4);
            buf.push_back(std::get<bool>(v) ? 1 : 0);
        }
    }
    return buf;
}

std::vector<Value> Tuple::deserialize(const char *data, size_t len, size_t num_columns) {
    std::vector<Value> values;
    values.reserve(num_columns);
    size_t pos = 0;
    for (size_t i = 0; i < num_columns; i++) {
        need(pos, 1, len);
        uint8_t tag = static_cast<uint8_t>(data[pos++]);
        switch (tag) {
            case 0: values.emplace_back(std::monostate{}); break;
            case 1: {
                need(pos, 8, len);
                int64_t v; std::memcpy(&v, data + pos, 8); pos += 8;
                values.emplace_back(v);
                break;
            }
            case 2: {
                need(pos, 8, len);
                double v; std::memcpy(&v, data + pos, 8); pos += 8;
                values.emplace_back(v);
                break;
            }
            case 3: {
                need(pos, 4, len);
                uint32_t n; std::memcpy(&n, data + pos, 4); pos += 4;
                need(pos, n, len);
                values.emplace_back(std::string(data + pos, n));
                pos += n;
                break;
            }
            case 4:
                need(pos, 1, len);
                values.emplace_back(data[pos++] != 0);
                break;
            default:
                throw std::runtime_error("Tuple: corrupt row (bad type tag)");
        }
    }
    return values;
}
