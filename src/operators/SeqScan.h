#pragma once
#include <optional>
#include "Operator.h"
#include "../engine/Database.h"

// Reads every row of a table, page by page, slot by slot.
class SeqScan : public Operator {
public:
    SeqScan(Database &db, const std::string &table, const std::string &alias);
    void open() override;
    std::string name() const override { return "SeqScan"; }
    std::string detail() const override;

protected:
    bool next(Row &out) override;

private:
    Database                         &db_;
    std::string                       table_, alias_;
    size_t                            ncols_;
    std::optional<HeapFile::Cursor>   cursor_;
    std::vector<char>                 buf_;
};

// Builds the schema of a base table, labelled with the alias used in the query.
Schema table_schema(const TableDef &def, const std::string &alias);
