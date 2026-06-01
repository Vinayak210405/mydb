#include "Tuple.h"

// Format per field:
//   1 byte: type tag (0=null, 1=int, 2=float, 3=string, 4=bool)
//   then:
//     int:    8 bytes
//     float:  8 bytes
//     string: 4 bytes length + N bytes data
//     bool:   1 byte
//     null:   0 bytes

std::vector<char> Tuple::serialize(const std::vector<Value> &values) {
    std::vector<char> buf;
    for (auto &v : values) {
        if (std::holds_alternative<std::monostate>(v)) {
            buf.push_back(0);
        } else if (std::holds_alternative<int64_t>(v)) {
            buf.push_back(1);
            int64_t val = std::get<int64_t>(v);
            buf.resize(buf.size() + 8);
            std::memcpy(buf.data() + buf.size() - 8, &val, 8);
        } else if (std::holds_alternative<double>(v)) {
            buf.push_back(2);
            double val = std::get<double>(v);
            buf.resize(buf.size() + 8);
            std::memcpy(buf.data() + buf.size() - 8, &val, 8);
        } else if (std::holds_alternative<std::string>(v)) {
            buf.push_back(3);
            const std::string &s = std::get<std::string>(v);
            uint32_t slen = static_cast<uint32_t>(s.size());
            buf.resize(buf.size() + 4);
            std::memcpy(buf.data() + buf.size() - 4, &slen, 4);
            buf.insert(buf.end(), s.begin(), s.end());
        } else if (std::holds_alternative<bool>(v)) {
            buf.push_back(4);
            buf.push_back(std::get<bool>(v) ? 1 : 0);
        }
    }
    return buf;
}

std::vector<Value> Tuple::deserialize(const char *data, uint16_t len,
                                      const std::vector<DataType> &types) {
    std::vector<Value> values;
    size_t pos = 0;
    for (size_t i = 0; i < types.size() && pos < len; i++) {
        uint8_t tag = static_cast<uint8_t>(data[pos++]);
        if (tag == 0) {
            values.emplace_back(std::monostate{});
        } else if (tag == 1) {
            int64_t val;
            std::memcpy(&val, data + pos, 8); pos += 8;
            values.emplace_back(val);
        } else if (tag == 2) {
            double val;
            std::memcpy(&val, data + pos, 8); pos += 8;
            values.emplace_back(val);
        } else if (tag == 3) {
            uint32_t slen;
            std::memcpy(&slen, data + pos, 4); pos += 4;
            std::string s(data + pos, slen); pos += slen;
            values.emplace_back(std::move(s));
        } else if (tag == 4) {
            values.emplace_back(data[pos++] != 0);
        }
    }
    return values;
}
