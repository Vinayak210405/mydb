#pragma once
#include <string>
#include <vector>
#include "Column.h"

struct TableDef {
    std::string         name;
    std::vector<Column> columns;

    TableDef() = default;
    explicit TableDef(std::string name) : name(std::move(name)) {}

    TableDef& add_column(Column col) {
        columns.push_back(std::move(col));
        return *this;
    }

    const Column* get_column(const std::string &col_name) const {
        for (auto &c : columns)
            if (c.name == col_name) return &c;
        return nullptr;
    }

    int column_index(const std::string &col_name) const {
        for (int i = 0; i < (int)columns.size(); i++)
            if (columns[i].name == col_name) return i;
        return -1;
    }

    std::string to_str() const {
        std::string s = "TABLE " + name + " (\n";
        for (size_t i = 0; i < columns.size(); i++) {
            s += "  " + columns[i].to_str();
            if (i + 1 < columns.size()) s += ",";
            s += "\n";
        }
        return s + ")";
    }
};
