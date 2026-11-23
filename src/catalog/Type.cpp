#include "Type.h"
#include <stdexcept>
#include <sstream>
#include <cmath>
#include <functional>

std::string value_to_str(const Value &v) {
    if (is_null(v))   return "NULL";
    if (is_int(v))    return std::to_string(std::get<int64_t>(v));
    if (is_float(v)) {
        std::ostringstream ss;
        ss.precision(15);
        double d = std::get<double>(v);
        ss << d;
        std::string out = ss.str();
        // Keep floats recognisable: 250 → 250.0
        if (std::isfinite(d) && out.find_first_of(".eEn") == std::string::npos) out += ".0";
        return out;
    }
    if (is_string(v)) return std::get<std::string>(v);
    if (is_bool(v))   return std::get<bool>(v) ? "true" : "false";
    return "?";
}

const char* value_type_name(const Value &v) {
    if (is_null(v))   return "NULL";
    if (is_int(v))    return "INT";
    if (is_float(v))  return "FLOAT";
    if (is_string(v)) return "VARCHAR";
    return "BOOL";
}

int compare_values(const Value &a, const Value &b) {
    if (is_numeric(a) && is_numeric(b)) {
        if (is_int(a) && is_int(b)) {
            int64_t x = std::get<int64_t>(a), y = std::get<int64_t>(b);
            return (x < y) ? -1 : (x > y ? 1 : 0);
        }
        double x = as_double(a), y = as_double(b);
        return (x < y) ? -1 : (x > y ? 1 : 0);
    }
    if (is_string(a) && is_string(b)) {
        int c = std::get<std::string>(a).compare(std::get<std::string>(b));
        return (c < 0) ? -1 : (c > 0 ? 1 : 0);
    }
    if (is_bool(a) && is_bool(b)) {
        return static_cast<int>(std::get<bool>(a)) - static_cast<int>(std::get<bool>(b));
    }
    throw std::runtime_error(std::string("cannot compare ") + value_type_name(a) +
                             " with " + value_type_name(b));
}

bool values_equal(const Value &a, const Value &b) {
    if (is_null(a) || is_null(b)) return is_null(a) && is_null(b);
    if (is_numeric(a) != is_numeric(b)) return false;
    if (!is_numeric(a) && a.index() != b.index()) return false;
    return compare_values(a, b) == 0;
}

size_t hash_value(const Value &v) {
    if (is_null(v))   return 0x9e3779b9;
    if (is_int(v))    return std::hash<double>{}(static_cast<double>(std::get<int64_t>(v)));
    if (is_float(v))  return std::hash<double>{}(std::get<double>(v));
    if (is_string(v)) return std::hash<std::string>{}(std::get<std::string>(v));
    return std::hash<bool>{}(std::get<bool>(v));
}

std::optional<Value> coerce_to(const Value &v, DataType t) {
    if (is_null(v)) return v;
    switch (t) {
        case DataType::INT:
            if (is_int(v)) return v;
            if (is_float(v)) {
                double d = std::get<double>(v);
                if (std::floor(d) == d) return Value{static_cast<int64_t>(d)};
            }
            return std::nullopt;
        case DataType::FLOAT:
            if (is_numeric(v)) return Value{as_double(v)};
            return std::nullopt;
        case DataType::VARCHAR:
            if (is_string(v)) return v;
            return std::nullopt;
        case DataType::BOOL:
            if (is_bool(v)) return v;
            return std::nullopt;
        default:
            return std::nullopt;
    }
}
