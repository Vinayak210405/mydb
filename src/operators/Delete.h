#pragma once
#include "Operator.h"
#include "../engine/Database.h"

// DELETE: removes the rows produced by the child from the heap file and
// from every index. Produces a single output row: [number of rows deleted].
class Delete : public Operator {
public:
    Delete(Database &db, const std::string &table, OperatorPtr child);
    void open() override { done_ = false; children_[0]->open(); }
    void close() override { children_[0]->close(); }
    std::string name() const override { return "Delete"; }
    std::string detail() const override { return table_; }

protected:
    bool next(Row &out) override;

private:
    Database    &db_;
    std::string  table_;
    bool         done_ = false;
};
