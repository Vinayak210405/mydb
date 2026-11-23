#pragma once
#include "Operator.h"

// ORDER BY: reads all input rows into memory, sorts them, then emits them.
// NULLs sort first in ascending order (last in descending).
class Sort : public Operator {
public:
    Sort(OperatorPtr child, const std::vector<ExprPtr> &keys, const std::vector<bool> &desc);
    void open() override;
    void close() override { children_[0]->close(); rows_.clear(); }
    std::string name() const override { return "Sort"; }
    std::string detail() const override;

protected:
    bool next(Row &out) override;

private:
    std::vector<ExprPtr> keys_;
    std::vector<bool>    desc_;
    std::vector<Row>     rows_;
    size_t               pos_ = 0;
};
