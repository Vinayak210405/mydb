#include "SeqScan.h"
#include "../storage/Tuple.h"
#include <stdexcept>

Schema table_schema(const TableDef &def, const std::string &alias) {
    Schema s;
    for (auto &c : def.columns) s.push_back({alias, c.name, c.type});
    return s;
}

SeqScan::SeqScan(Database &db, const std::string &table, const std::string &alias)
    : db_(db), table_(table), alias_(alias) {
    const TableDef *def = db.catalog().get_table(table);
    if (!def) throw std::runtime_error("table '" + table + "' not found");
    schema_ = table_schema(*def, alias);
    ncols_  = def->columns.size();
}

void SeqScan::open() {
    cursor_.emplace(db_.heap(table_));
}

bool SeqScan::next(Row &out) {
    if (!cursor_->next(out.rid, buf_)) return false;
    out.values = Tuple::deserialize(buf_.data(), buf_.size(), ncols_);
    return true;
}

std::string SeqScan::detail() const {
    return alias_ == table_ ? table_ : table_ + " AS " + alias_;
}
