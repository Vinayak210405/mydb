#include "Update.h"
#include "TableWriter.h"
#include "../storage/Tuple.h"
#include <stdexcept>

Update::Update(Database &db, const std::string &table, OperatorPtr child,
               const std::vector<std::pair<std::string, ExprPtr>> &assignments)
    : db_(db), table_(table) {
    const TableDef *def = db.catalog().get_table(table);
    for (auto &[col, expr] : assignments) {
        int idx = def->column_index(col);
        if (idx < 0) throw std::runtime_error("unknown column '" + col + "' in SET");
        assignments_.emplace_back(idx, bind_expr(expr, child->schema()));
    }
    schema_ = {{"", "rows_affected", DataType::INT}};
    children_.push_back(std::move(child));
}

bool Update::next(Row &out) {
    if (done_) return false;
    done_ = true;
    const TableDef &def = *db_.catalog().get_table(table_);
    HeapFile &heap = db_.heap(table_);

    std::vector<Row> targets;
    Row r;
    while (children_[0]->get_next(r)) targets.push_back(r);

    int64_t n = 0;
    for (auto &old_row : targets) {
        std::vector<Value> new_row = old_row.values;
        for (auto &[idx, expr] : assignments_) new_row[idx] = eval_expr(*expr, old_row.values);
        new_row = table_writer::prepare_row(def, new_row);
        table_writer::check_primary_key(db_, def, new_row, old_row.rid);

        auto bytes = Tuple::serialize(new_row);
        RID new_rid = heap.update(old_row.rid, bytes.data(), static_cast<uint16_t>(bytes.size()));

        // Index maintenance: an entry changes if the key OR the row's location changed.
        for (auto &col : def.indexed_columns) {
            int i = def.column_index(col);
            bool key_changed = !values_equal(old_row.values[i], new_row[i]);
            if (!key_changed && new_rid == old_row.rid) continue;
            BTree *idx = db_.index(table_, col);
            if (is_int(old_row.values[i])) idx->remove(std::get<int64_t>(old_row.values[i]), old_row.rid);
            if (is_int(new_row[i]))        idx->insert(std::get<int64_t>(new_row[i]), new_rid);
        }
        n++;
    }
    out.values = {n};
    out.rid = RID{};
    return true;
}

std::string Update::detail() const {
    const TableDef *def = db_.catalog().get_table(table_);
    std::string s = table_ + " SET ";
    for (size_t i = 0; i < assignments_.size(); i++) {
        if (i) s += ", ";
        s += def->columns[assignments_[i].first].name + " = " + assignments_[i].second->to_string();
    }
    return s;
}
