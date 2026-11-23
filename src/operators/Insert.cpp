#include "Insert.h"
#include "TableWriter.h"
#include "../storage/Tuple.h"

Insert::Insert(Database &db, const std::string &table, std::vector<std::vector<Value>> rows)
    : db_(db), table_(table), rows_(std::move(rows)) {
    schema_ = {{"", "rows_affected", DataType::INT}};
}

bool Insert::next(Row &out) {
    if (done_) return false;
    done_ = true;
    const TableDef &def = *db_.catalog().get_table(table_);
    HeapFile &heap = db_.heap(table_);
    int64_t n = 0;
    for (auto &raw : rows_) {
        auto row = table_writer::prepare_row(def, raw);
        table_writer::check_primary_key(db_, def, row);
        auto bytes = Tuple::serialize(row);
        RID rid = heap.insert(bytes.data(), static_cast<uint16_t>(bytes.size()));
        table_writer::add_to_indexes(db_, def, row, rid);
        n++;
    }
    out.values = {n};
    out.rid = RID{};
    return true;
}

std::string Insert::detail() const {
    return table_ + " (" + std::to_string(rows_.size()) + " row" + (rows_.size() == 1 ? "" : "s") + ")";
}
