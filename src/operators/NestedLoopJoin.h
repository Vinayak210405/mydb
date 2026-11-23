#pragma once
#include "Operator.h"

// For every left row, try every right row; keep pairs where the predicate is
// TRUE (or all pairs if there is no predicate — a cross join).
// The right side is read once and kept in memory.
class NestedLoopJoin : public Operator {
public:
    NestedLoopJoin(OperatorPtr left, OperatorPtr right, ExprPtr predicate);
    void open() override;
    void close() override;
    std::string name() const override { return "NestedLoopJoin"; }
    std::string detail() const override { return predicate_ ? predicate_->to_string() : "cross join"; }

protected:
    bool next(Row &out) override;

private:
    ExprPtr          predicate_;
    std::vector<Row> right_rows_;
    Row              left_row_;
    bool             have_left_ = false;
    size_t           right_pos_ = 0;
};

// Concatenates two schemas (left columns first).
Schema concat_schemas(const Schema &a, const Schema &b);
