#pragma once
#include <memory>
#include <string>
#include <vector>
#include "../query/Expression.h"
#include "../storage/RID.h"

// A row flowing between operators. `rid` is set by scans so that UPDATE and
// DELETE know which stored row to change; operators that build new rows
// (joins, projections, aggregates) leave it invalid.
struct Row {
    std::vector<Value> values;
    RID                rid;
};

// ---------------------------------------------------------------------------
// Operator — the Volcano / iterator model.
//
// A query plan is a tree of operators. The root's next() pulls a row from its
// children, which pull from theirs, down to the scans that read the table.
// Rows are produced one at a time, so a LIMIT 10 stops the scan early.
//
//   open()   prepare (e.g. build a hash table, open a cursor)
//   next(r)  produce the next row into r; false when exhausted
//   close()  release resources
// ---------------------------------------------------------------------------
class Operator {
public:
    virtual ~Operator() = default;

    virtual void open() = 0;
    virtual void close() {}

    // Wraps next() so every operator counts the rows it produced
    // (shown as "rows" in EXPLAIN-style plan output).
    bool get_next(Row &out) {
        if (!next(out)) return false;
        rows_out_++;
        return true;
    }

    const Schema& schema() const { return schema_; }
    uint64_t rows_out() const { return rows_out_; }

    // For plan display
    virtual std::string name() const = 0;
    virtual std::string detail() const { return ""; }
    std::vector<Operator*> children() const {
        std::vector<Operator*> c;
        for (auto &ch : children_) c.push_back(ch.get());
        return c;
    }

protected:
    virtual bool next(Row &out) = 0;

    Schema                                 schema_;
    std::vector<std::unique_ptr<Operator>> children_;
    uint64_t                               rows_out_ = 0;
};

using OperatorPtr = std::unique_ptr<Operator>;
