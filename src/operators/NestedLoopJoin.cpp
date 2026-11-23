#include "NestedLoopJoin.h"

Schema concat_schemas(const Schema &a, const Schema &b) {
    Schema s = a;
    s.insert(s.end(), b.begin(), b.end());
    return s;
}

NestedLoopJoin::NestedLoopJoin(OperatorPtr left, OperatorPtr right, ExprPtr predicate) {
    schema_ = concat_schemas(left->schema(), right->schema());
    if (predicate) predicate_ = bind_expr(predicate, schema_);
    children_.push_back(std::move(left));
    children_.push_back(std::move(right));
}

void NestedLoopJoin::open() {
    children_[0]->open();
    children_[1]->open();
    right_rows_.clear();
    Row r;
    while (children_[1]->get_next(r)) right_rows_.push_back(r);
    have_left_ = false;
    right_pos_ = 0;
}

void NestedLoopJoin::close() {
    children_[0]->close();
    children_[1]->close();
    right_rows_.clear();
}

bool NestedLoopJoin::next(Row &out) {
    while (true) {
        if (!have_left_ || right_pos_ >= right_rows_.size()) {
            if (!children_[0]->get_next(left_row_)) return false;
            have_left_ = true;
            right_pos_ = 0;
        }
        while (right_pos_ < right_rows_.size()) {
            const Row &r = right_rows_[right_pos_++];
            out.values = left_row_.values;
            out.values.insert(out.values.end(), r.values.begin(), r.values.end());
            out.rid = RID{};
            if (!predicate_ || is_true(eval_expr(*predicate_, out.values))) return true;
        }
    }
}
