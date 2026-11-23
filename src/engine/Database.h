#pragma once
#include <string>
#include <map>
#include <memory>
#include "../catalog/Catalog.h"
#include "../storage/HeapFile.h"
#include "../index/BTree.h"

// ---------------------------------------------------------------------------
// Database — owns everything on disk under one data directory:
//
//   data/catalog.meta        schema (tables, columns, indexes)
//   data/<table>.tbl         heap file with the table's rows
//   data/<table>.<col>.idx   B+ tree index on an INT column
//
// Each file has its own DiskManager + BufferPool, so tables never see each
// other's pages.
// ---------------------------------------------------------------------------
class Database {
public:
    explicit Database(std::string data_dir);
    ~Database();

    Catalog&       catalog()       { return catalog_; }
    const Catalog& catalog() const { return catalog_; }
    const std::string& data_dir() const { return dir_; }

    HeapFile& heap(const std::string &table);
    BTree*    index(const std::string &table, const std::string &column);

    // DDL — each one persists the catalog before returning.
    void create_table(const TableDef &def);
    void drop_table(const std::string &table);
    // Builds the index from the table's existing rows.
    void create_index(const std::string &table, const std::string &column);
    void drop_index(const std::string &table, const std::string &column);

    void flush_all();

    std::string table_path(const std::string &table) const;
    std::string index_path(const std::string &table, const std::string &column) const;

private:
    void save_catalog();

    std::string                                       dir_;
    Catalog                                           catalog_;
    std::map<std::string, std::unique_ptr<HeapFile>>  heaps_;
    std::map<std::string, std::unique_ptr<BTree>>     indexes_;   // key "table.column"
};
