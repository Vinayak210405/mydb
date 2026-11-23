#pragma once
#include <unordered_map>
#include "Operator.h"
#include "Limit.h"

// GROUP BY + aggregate functions (COUNT, SUM, AVG, MIN, MAX), using a hash
// table from group key → running aggregate state.
//
// Output row = [group expressions..., aggregate results...]. With no GROUP BY
// there is exactly one output row, even for an empty input (COUNT(*) = 0).
struct AggSpec {
    std::string func;            // COUNT, SUM, AVG, MIN, MAX
    ExprPtr     arg;             // null for COUNT(*)
    bool        distinct = false;
    std::string label;           // e.g. "COUNT(*)"
};

class Aggregate : public Operator {
public:
    Aggregate(OperatorPtr child, const std::vector<ExprPtr> &group_by,
              const std::vector<AggSpec> &aggs);
    void open() override;
    void close() override { children_[0]->close(); results_.clear(); }
    std::string name() const override { return "Aggregate"; }
    std::string detail() const override;

protected:
    bool next(Row &out) override;

private:
    struct State {
        int64_t count = 0;
        Value   acc;                                  // SUM / MIN / MAX
        double  sum   = 0;                            // AVG
        std::unordered_map<std::vector<Value>, bool, RowHash, RowEqual> seen;   // DISTINCT
    };

    std::vector<ExprPtr> group_by_;
    std::vector<AggSpec> aggs_;
    std::vector<Row>     results_;
    size_t               pos_ = 0;
};
