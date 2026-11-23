#pragma once
#include "Operator.h"
#include "../engine/Database.h"

// UPDATE: the child operator (a scan, maybe filtered) finds the rows; this
// operator rewrites them and fixes up every index. All matching rows are
// collected BEFORE any is changed, so a row that moves can't be updated
// twice (the "Halloween problem").
// Produces a single output row: [number of rows updated].
class Update : public Operator {
public:
    Update(Database &db, const std::string &table, OperatorPtr child,
           const std::vector<std::pair<std::string, ExprPtr>> &assignments);
    void open() override { done_ = false; children_[0]->open(); }
    void close() override { children_[0]->close(); }
    std::string name() const override { return "Update"; }
    std::string detail() const override;

protected:
    bool next(Row &out) override;

private:
    Database                             &db_;
    std::string                           table_;
    std::vector<std::pair<int, ExprPtr>>  assignments_;   // column index → bound expression
    bool                                  done_ = false;
};
