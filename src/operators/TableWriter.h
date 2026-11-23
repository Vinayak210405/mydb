#pragma once
#include <vector>
#include "../engine/Database.h"

// Shared rules for every write (INSERT and UPDATE):
//   * each value is converted to its column's type, or rejected
//   * NOT NULL columns may not be NULL
//   * PRIMARY KEY values must be unique
//   * every index on the table is kept in sync with the heap file
namespace table_writer {

// Throws a descriptive error if `row` is not valid for the table.
std::vector<Value> prepare_row(const TableDef &def, std::vector<Value> row);

// Throws if another row (not `self`) already has this row's primary key.
void check_primary_key(Database &db, const TableDef &def,
                       const std::vector<Value> &row, RID self = RID{});

void add_to_indexes(Database &db, const TableDef &def, const std::vector<Value> &row, RID rid);
void remove_from_indexes(Database &db, const TableDef &def, const std::vector<Value> &row, RID rid);

}  // namespace table_writer
