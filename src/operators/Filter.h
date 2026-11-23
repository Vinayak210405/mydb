#pragma once
#include "Operator.h"

// Passes through only the rows for which the predicate is TRUE.
class Filter : public Operator {
public:
    Filter(OperatorPtr child, ExprPtr predicate);   // predicate is bound here
    void open() override  { children_[0]->open(); }
    void close() override { children_[0]->close(); }
    std::string name() const override { return "Filter"; }
    std::string detail() const override { return predicate_->to_string(); }

protected:
    bool next(Row &out) override;

private:
    ExprPtr predicate_;
};
