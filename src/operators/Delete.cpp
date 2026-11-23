#include "Delete.h"
#include "TableWriter.h"

Delete::Delete(Database &db, const std::string &table, OperatorPtr child)
    : db_(db), table_(table) {
    schema_ = {{"", "rows_affected", DataType::INT}};
    children_.push_back(std::move(child));
}

bool Delete::next(Row &out) {
    if (done_) return false;
    done_ = true;
    const TableDef &def = *db_.catalog().get_table(table_);
    HeapFile &heap = db_.heap(table_);

    std::vector<Row> targets;
    Row r;
    while (children_[0]->get_next(r)) targets.push_back(r);

    int64_t n = 0;
    for (auto &row : targets) {
        if (!heap.remove(row.rid)) continue;
        table_writer::remove_from_indexes(db_, def, row.values, row.rid);
        n++;
    }
    out.values = {n};
    out.rid = RID{};
    return true;
}
