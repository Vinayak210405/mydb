#pragma once
#include <cstdint>
#include <string>
#include <variant>
#include <optional>

// ---------------------------------------------------------------------------
// SQL data types and runtime values
// ---------------------------------------------------------------------------
enum class DataType : uint8_t { INT, FLOAT, VARCHAR, BOOL, NULLTYPE };

inline const char* dtype_str(DataType t) {
    switch (t) {
        case DataType::INT:     return "INT";
        case DataType::FLOAT:   return "FLOAT";
        case DataType::VARCHAR: return "VARCHAR";
        case DataType::BOOL:    return "BOOL";
        default:                return "NULL";
    }
}

inline std::optional<DataType> dtype_from_str(const std::string &s) {
    if (s == "INT")     return DataType::INT;
    if (s == "FLOAT")   return DataType::FLOAT;
    if (s == "VARCHAR") return DataType::VARCHAR;
    if (s == "BOOL")    return DataType::BOOL;
    return std::nullopt;
}

// A single SQL value. std::monostate represents NULL.
using Value = std::variant<std::monostate, int64_t, double, std::string, bool>;

inline bool is_null(const Value &v)   { return std::holds_alternative<std::monostate>(v); }
inline bool is_int(const Value &v)    { return std::holds_alternative<int64_t>(v); }
inline bool is_float(const Value &v)  { return std::holds_alternative<double>(v); }
inline bool is_string(const Value &v) { return std::holds_alternative<std::string>(v); }
inline bool is_bool(const Value &v)   { return std::holds_alternative<bool>(v); }
inline bool is_numeric(const Value &v){ return is_int(v) || is_float(v); }

inline double as_double(const Value &v) {
    return is_int(v) ? static_cast<double>(std::get<int64_t>(v)) : std::get<double>(v);
}

std::string value_to_str(const Value &v);
const char* value_type_name(const Value &v);

// Three-way comparison of two non-NULL values.
// Returns <0, 0, >0. Throws std::runtime_error if the types are not comparable.
int compare_values(const Value &a, const Value &b);

// Equality used by hashing / grouping (NULL == NULL here, unlike SQL '=').
bool values_equal(const Value &a, const Value &b);
size_t hash_value(const Value &v);

// Convert a value so it can be stored in a column of type `t`.
// Returns std::nullopt if the value is not compatible (e.g. 'abc' into INT).
std::optional<Value> coerce_to(const Value &v, DataType t);
