#pragma once
#include <string>
#include <map>
#include <vector>
#include "TableDef.h"

// ---------------------------------------------------------------------------
// Catalog — the database's metadata: which tables exist, their columns, and
// which columns are indexed. It is saved to a small text file
// (data/catalog.meta) after every DDL statement and loaded on startup, so the
// schema survives a restart.
//
// Thread safety: the Catalog is only touched while the QueryEngine holds its
// statement lock, so it has no lock of its own.
// ---------------------------------------------------------------------------
class Catalog {
public:
    bool create_table(TableDef def);
    bool drop_table(const std::string &name);
    TableDef*       get_table(const std::string &name);
    const TableDef* get_table(const std::string &name) const;
    bool table_exists(const std::string &name) const;
    std::vector<std::string> list_tables() const;
    size_t table_count() const { return tables_.size(); }

    void save(const std::string &path) const;
    void load(const std::string &path);   // no-op if the file does not exist

private:
    std::map<std::string, TableDef> tables_;   // ordered → stable SHOW TABLES
};
