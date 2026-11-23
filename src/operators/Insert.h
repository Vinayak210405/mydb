#pragma once
#include "Operator.h"
#include "../engine/Database.h"

// INSERT: writes rows into the heap file and every index.
// Produces a single output row: [number of rows inserted].
class Insert : public Operator {
public:
    Insert(Database &db, const std::string &table, std::vector<std::vector<Value>> rows);
    void open() override { done_ = false; }
    std::string name() const override { return "Insert"; }
    std::string detail() const override;

protected:
    bool next(Row &out) override;

private:
    Database                        &db_;
    std::string                      table_;
    std::vector<std::vector<Value>>  rows_;
    bool                             done_ = false;
};
