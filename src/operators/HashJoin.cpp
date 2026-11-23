#include "HashJoin.h"
#include "NestedLoopJoin.h"

HashJoin::HashJoin(OperatorPtr left, OperatorPtr right,
                   std::vector<ExprPtr> left_keys, std::vector<ExprPtr> right_keys,
                   ExprPtr residual) {
    for (auto &k : left_keys)  left_keys_.push_back(bind_expr(k, left->schema()));
    for (auto &k : right_keys) right_keys_.push_back(bind_expr(k, right->schema()));
    schema_ = concat_schemas(left->schema(), right->schema());
    if (residual) residual_ = bind_expr(residual, schema_);
    children_.push_back(std::move(left));
    children_.push_back(std::move(right));
}

static bool eval_key(const std::vector<ExprPtr> &keys, const std::vector<Value> &row,
                     std::vector<Value> &out) {
    out.clear();
    for (auto &k : keys) {
        Value v = eval_expr(*k, row);
        if (is_null(v)) return false;          // NULL never equals anything
        if (is_int(v)) v = static_cast<double>(std::get<int64_t>(v));   // 1 joins with 1.0
        out.push_back(std::move(v));
    }
    return true;
}

void HashJoin::open() {
    children_[0]->open();
    children_[1]->open();
    table_.clear();
    Row r;
    std::vector<Value> key;
    while (children_[1]->get_next(r))                  // build phase
        if (eval_key(right_keys_, r.values, key)) table_[key].push_back(r);
    matches_ = nullptr;
    match_pos_ = 0;
}

void HashJoin::close() {
    children_[0]->close();
    children_[1]->close();
    table_.clear();
}

bool HashJoin::next(Row &out) {
    std::vector<Value> key;
    while (true) {
        if (matches_ && match_pos_ < matches_->size()) {
            const Row &r = (*matches_)[match_pos_++];
            out.values = left_row_.values;
            out.values.insert(out.values.end(), r.values.begin(), r.values.end());
            out.rid = RID{};
            if (!residual_ || is_true(eval_expr(*residual_, out.values))) return true;
            continue;
        }
        if (!children_[0]->get_next(left_row_)) return false;   // probe phase
        matches_ = nullptr;
        match_pos_ = 0;
        if (!eval_key(left_keys_, left_row_.values, key)) continue;
        auto it = table_.find(key);
        if (it != table_.end()) matches_ = &it->second;
    }
}

std::string HashJoin::detail() const {
    const Schema &ls = children_[0]->schema();
    const Schema &rs = children_[1]->schema();
    auto col = [](const ExprPtr &e, const Schema &s) {
        if (e->type == ExprType::COLUMN && e->slot >= 0) {
            const auto &c = s[e->slot];
            return c.table.empty() ? c.name : c.table + "." + c.name;
        }
        return e->to_string();
    };
    std::string d;
    for (size_t i = 0; i < left_keys_.size(); i++) {
        if (i) d += " AND ";
        d += col(left_keys_[i], ls) + " = " + col(right_keys_[i], rs);
    }
    if (residual_) d += " (then " + residual_->to_string() + ")";
    return d + "; hash table built on the right input";
}
