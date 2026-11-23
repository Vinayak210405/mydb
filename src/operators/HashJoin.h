#pragma once
#include <unordered_map>
#include "Operator.h"
#include "Limit.h"

// Equi-join: build a hash table on the right input keyed by the join
// columns, then stream the left input and probe it. O(left + right) instead
// of the nested loop's O(left × right).
class HashJoin : public Operator {
public:
    // left_keys are bound to the left schema, right_keys to the right schema;
    // `residual` (optional) is checked on the joined row.
    HashJoin(OperatorPtr left, OperatorPtr right,
             std::vector<ExprPtr> left_keys, std::vector<ExprPtr> right_keys,
             ExprPtr residual);
    void open() override;
    void close() override;
    std::string name() const override { return "HashJoin"; }
    std::string detail() const override;

protected:
    bool next(Row &out) override;

private:
    std::vector<ExprPtr> left_keys_, right_keys_;
    ExprPtr              residual_;
    std::unordered_map<std::vector<Value>, std::vector<Row>, RowHash, RowEqual> table_;
    Row                     left_row_;
    const std::vector<Row> *matches_ = nullptr;
    size_t                  match_pos_ = 0;
};
