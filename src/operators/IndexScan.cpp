#include "IndexScan.h"
#include "SeqScan.h"
#include "../storage/Tuple.h"
#include <stdexcept>

IndexScan::IndexScan(Database &db, const std::string &table, const std::string &alias,
                     const std::string &column, int64_t low, int64_t high)
    : db_(db), table_(table), alias_(alias), column_(column), low_(low), high_(high) {
    const TableDef *def = db.catalog().get_table(table);
    if (!def) throw std::runtime_error("table '" + table + "' not found");
    schema_ = table_schema(*def, alias);
    ncols_  = def->columns.size();
}

void IndexScan::open() {
    BTree *idx = db_.index(table_, column_);
    if (!idx) throw std::runtime_error("index on " + table_ + "." + column_ + " disappeared");
    rids_ = idx->range(low_, high_);
    pos_ = 0;
}

bool IndexScan::next(Row &out) {
    HeapFile &heap = db_.heap(table_);
    while (pos_ < rids_.size()) {
        RID rid = rids_[pos_++];
        if (!heap.fetch(rid, buf_)) continue;
        out.rid = rid;
        out.values = Tuple::deserialize(buf_.data(), buf_.size(), ncols_);
        return true;
    }
    return false;
}

std::string IndexScan::detail() const {
    std::string t = alias_ == table_ ? table_ : table_ + " AS " + alias_;
    std::string cond;
    if (low_ == high_) cond = column_ + " = " + std::to_string(low_);
    else if (low_ == INT64_MIN) cond = column_ + " <= " + std::to_string(high_);
    else if (high_ == INT64_MAX) cond = column_ + " >= " + std::to_string(low_);
    else cond = std::to_string(low_) + " <= " + column_ + " <= " + std::to_string(high_);
    return t + " using B+ tree on " + column_ + " (" + cond + ")";
}
