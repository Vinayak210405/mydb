#include "Database.h"
#include "../storage/Tuple.h"
#include <filesystem>
#include <stdexcept>

namespace fs = std::filesystem;

Database::Database(std::string data_dir) : dir_(std::move(data_dir)) {
    fs::create_directories(dir_);
    catalog_.load((fs::path(dir_) / "catalog.meta").string());
}

Database::~Database() {
    try { flush_all(); } catch (...) {}
}

std::string Database::table_path(const std::string &table) const {
    return (fs::path(dir_) / (table + ".tbl")).string();
}

std::string Database::index_path(const std::string &table, const std::string &column) const {
    return (fs::path(dir_) / (table + "." + column + ".idx")).string();
}

void Database::save_catalog() {
    catalog_.save((fs::path(dir_) / "catalog.meta").string());
}

HeapFile& Database::heap(const std::string &table) {
    if (!catalog_.table_exists(table))
        throw std::runtime_error("table '" + table + "' not found");
    auto it = heaps_.find(table);
    if (it != heaps_.end()) return *it->second;
    auto hf = std::make_unique<HeapFile>(table_path(table));
    HeapFile &ref = *hf;
    heaps_[table] = std::move(hf);
    return ref;
}

BTree* Database::index(const std::string &table, const std::string &column) {
    const TableDef *def = catalog_.get_table(table);
    if (!def || !def->has_index(column)) return nullptr;
    std::string key = table + "." + column;
    auto it = indexes_.find(key);
    if (it != indexes_.end()) return it->second.get();
    auto bt = std::make_unique<BTree>(index_path(table, column));
    BTree *ptr = bt.get();
    indexes_[key] = std::move(bt);
    return ptr;
}

void Database::create_table(const TableDef &def) {
    if (catalog_.table_exists(def.name))
        throw std::runtime_error("table '" + def.name + "' already exists");
    // Leftover files from an earlier table with the same name must not leak in.
    std::error_code ec;
    fs::remove(table_path(def.name), ec);
    for (auto &c : def.columns) fs::remove(index_path(def.name, c.name), ec);

    TableDef copy = def;
    copy.indexed_columns.clear();
    catalog_.create_table(copy);
    save_catalog();

    // An INT primary key gets an index automatically (used for lookups and
    // for enforcing uniqueness).
    for (auto &c : def.columns)
        if (c.is_primary_key && c.type == DataType::INT)
            create_index(def.name, c.name);
}

void Database::drop_table(const std::string &table) {
    TableDef *def = catalog_.get_table(table);
    if (!def) throw std::runtime_error("table '" + table + "' not found");
    std::vector<std::string> cols = def->indexed_columns;

    heaps_.erase(table);                      // closes the file
    for (auto &c : cols) indexes_.erase(table + "." + c);

    catalog_.drop_table(table);
    save_catalog();

    std::error_code ec;
    fs::remove(table_path(table), ec);
    for (auto &c : cols) fs::remove(index_path(table, c), ec);
}

void Database::create_index(const std::string &table, const std::string &column) {
    TableDef *def = catalog_.get_table(table);
    if (!def) throw std::runtime_error("table '" + table + "' not found");
    int col = def->column_index(column);
    if (col < 0) throw std::runtime_error("column '" + column + "' not found in '" + table + "'");
    if (def->columns[col].type != DataType::INT)
        throw std::runtime_error("indexes are only supported on INT columns ('" +
                                 column + "' is " + dtype_str(def->columns[col].type) + ")");
    if (def->has_index(column))
        throw std::runtime_error("index on " + table + "." + column + " already exists");

    std::error_code ec;
    fs::remove(index_path(table, column), ec);
    auto bt = std::make_unique<BTree>(index_path(table, column));

    size_t ncols = def->columns.size();
    heap(table).scan([&](RID rid, const char *data, uint16_t len) {
        auto row = Tuple::deserialize(data, len, ncols);
        if (is_int(row[col])) bt->insert(std::get<int64_t>(row[col]), rid);
    });
    bt->flush();
    indexes_[table + "." + column] = std::move(bt);

    def->indexed_columns.push_back(column);
    save_catalog();
}

void Database::drop_index(const std::string &table, const std::string &column) {
    TableDef *def = catalog_.get_table(table);
    if (!def) throw std::runtime_error("table '" + table + "' not found");
    if (!def->has_index(column))
        throw std::runtime_error("no index on " + table + "." + column);
    indexes_.erase(table + "." + column);
    auto &v = def->indexed_columns;
    v.erase(std::remove(v.begin(), v.end(), column), v.end());
    save_catalog();
    std::error_code ec;
    fs::remove(index_path(table, column), ec);
}

void Database::flush_all() {
    for (auto &[_, h] : heaps_)   h->flush();
    for (auto &[_, i] : indexes_) i->flush();
}
