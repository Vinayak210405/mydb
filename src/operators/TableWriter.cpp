#include "TableWriter.h"
#include "../storage/Tuple.h"
#include <stdexcept>

namespace table_writer {

std::vector<Value> prepare_row(const TableDef &def, std::vector<Value> row) {
    if (row.size() != def.columns.size())
        throw std::runtime_error("expected " + std::to_string(def.columns.size()) +
                                 " values, got " + std::to_string(row.size()));
    for (size_t i = 0; i < row.size(); i++) {
        const Column &c = def.columns[i];
        if (is_null(row[i])) {
            if (!c.nullable)
                throw std::runtime_error("column '" + c.name + "' cannot be NULL");
            continue;
        }
        auto v = coerce_to(row[i], c.type);
        if (!v)
            throw std::runtime_error("column '" + c.name + "' is " + dtype_str(c.type) +
                                     " but got " + value_type_name(row[i]) + " value " +
                                     value_to_str(row[i]));
        row[i] = *v;
    }
    return row;
}

void check_primary_key(Database &db, const TableDef &def,
                       const std::vector<Value> &row, RID self) {
    int pk = -1;
    for (int i = 0; i < (int)def.columns.size(); i++)
        if (def.columns[i].is_primary_key) pk = i;
    if (pk < 0) return;
    const Value &key = row[pk];
    auto duplicate = [&]() {
        throw std::runtime_error("duplicate primary key " + def.columns[pk].name + " = " +
                                 value_to_str(key));
    };

    if (BTree *idx = db.index(def.name, def.columns[pk].name); idx && is_int(key)) {
        for (RID r : idx->lookup(std::get<int64_t>(key)))
            if (r != self) duplicate();
        return;
    }
    // No index (non-INT key): fall back to scanning the table.
    db.heap(def.name).scan([&](RID rid, const char *data, uint16_t len) {
        if (rid == self) return;
        auto other = Tuple::deserialize(data, len, def.columns.size());
        if (values_equal(other[pk], key)) duplicate();
    });
}

void add_to_indexes(Database &db, const TableDef &def, const std::vector<Value> &row, RID rid) {
    for (auto &col : def.indexed_columns) {
        int i = def.column_index(col);
        if (i >= 0 && is_int(row[i]))
            db.index(def.name, col)->insert(std::get<int64_t>(row[i]), rid);
    }
}

void remove_from_indexes(Database &db, const TableDef &def, const std::vector<Value> &row, RID rid) {
    for (auto &col : def.indexed_columns) {
        int i = def.column_index(col);
        if (i >= 0 && is_int(row[i]))
            db.index(def.name, col)->remove(std::get<int64_t>(row[i]), rid);
    }
}

}  // namespace table_writer
