#pragma once
#include "Operator.h"

// Computes the SELECT list: one output column per expression.
class Projection : public Operator {
public:
    Projection(OperatorPtr child, const std::vector<ExprPtr> &exprs,
               const std::vector<std::string> &names);
    void open() override  { children_[0]->open(); }
    void close() override { children_[0]->close(); }
    std::string name() const override { return "Projection"; }
    std::string detail() const override;

protected:
    bool next(Row &out) override;

private:
    std::vector<ExprPtr> exprs_;
    Row                  in_;
};
