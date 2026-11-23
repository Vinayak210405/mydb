#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include "Column.h"

struct TableDef {
    std::string              name;
    std::vector<Column>      columns;
    std::vector<std::string> indexed_columns;   // columns that have a B+ tree index

    TableDef() = default;
    explicit TableDef(std::string name) : name(std::move(name)) {}

    TableDef& add_column(Column col) {
        columns.push_back(std::move(col));
        return *this;
    }

    const Column* get_column(const std::string &col_name) const {
        int i = column_index(col_name);
        return i < 0 ? nullptr : &columns[i];
    }

    int column_index(const std::string &col_name) const {
        for (int i = 0; i < (int)columns.size(); i++)
            if (columns[i].name == col_name) return i;
        return -1;
    }

    bool has_index(const std::string &col) const {
        return std::find(indexed_columns.begin(), indexed_columns.end(), col)
               != indexed_columns.end();
    }

    std::vector<DataType> types() const {
        std::vector<DataType> t;
        for (auto &c : columns) t.push_back(c.type);
        return t;
    }
};
