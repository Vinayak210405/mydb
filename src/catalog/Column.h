#pragma once
#include <string>
#include "Type.h"

struct Column {
    std::string name;
    DataType    type;
    bool        nullable       = true;
    bool        is_primary_key = false;

    Column() = default;
    Column(std::string name, DataType type, bool nullable = true)
        : name(std::move(name)), type(type), nullable(nullable) {}

    std::string to_str() const {
        std::string s = name + " " + dtype_str(type);
        if (!nullable)      s += " NOT NULL";
        if (is_primary_key) s += " PRIMARY KEY";
        return s;
    }
};
