#pragma once
#include <cstdint>
#include <string>
#include <variant>

enum class DataType : uint8_t { INT, FLOAT, VARCHAR, BOOL, NULLTYPE };

inline const char* dtype_str(DataType t) {
    switch(t) {
        case DataType::INT:     return "INT";
        case DataType::FLOAT:   return "FLOAT";
        case DataType::VARCHAR: return "VARCHAR";
        case DataType::BOOL:    return "BOOL";
        default:                return "NULL";
    }
}

using Value = std::variant<std::monostate, int64_t, double, std::string, bool>;

inline std::string value_to_str(const Value &v) {
    if (std::holds_alternative<std::monostate>(v)) return "NULL";
    if (std::holds_alternative<int64_t>(v))  return std::to_string(std::get<int64_t>(v));
    if (std::holds_alternative<double>(v))   return std::to_string(std::get<double>(v));
    if (std::holds_alternative<std::string>(v)) return std::get<std::string>(v);
    if (std::holds_alternative<bool>(v))     return std::get<bool>(v) ? "true" : "false";
    return "?";
}
