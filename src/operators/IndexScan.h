#pragma once
#include <climits>
#include "Operator.h"
#include "../engine/Database.h"

// Uses a B+ tree to find only the rows whose indexed column is in
// [low, high], then fetches each row from the heap file by its RID.
class IndexScan : public Operator {
public:
    IndexScan(Database &db, const std::string &table, const std::string &alias,
              const std::string &column, int64_t low, int64_t high);
    void open() override;
    std::string name() const override { return "IndexScan"; }
    std::string detail() const override;

protected:
    bool next(Row &out) override;

private:
    Database         &db_;
    std::string       table_, alias_, column_;
    int64_t           low_, high_;
    size_t            ncols_;
    std::vector<RID>  rids_;
    size_t            pos_ = 0;
    std::vector<char> buf_;
};
