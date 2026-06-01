#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include "TableDef.h"

class Catalog {
public:
    bool create_table(TableDef def);
    bool drop_table(const std::string &name);
    const TableDef* get_table(const std::string &name) const;
    bool table_exists(const std::string &name) const;
    std::vector<std::string> list_tables() const;
    size_t table_count() const;

private:
    mutable std::mutex                        mutex_;
    std::unordered_map<std::string, TableDef> tables_;
};
