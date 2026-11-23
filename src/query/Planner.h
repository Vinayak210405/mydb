#pragma once
#include <vector>
#include <string>
#include "AST.h"
#include "../operators/Operator.h"
#include "../engine/Database.h"

// ---------------------------------------------------------------------------
// Planner — turns a parsed statement into a tree of operators.
//
// SELECT is planned bottom-up, in the same order SQL evaluates the clauses:
//
//   scans (SeqScan / IndexScan + pushed-down Filters)      FROM, WHERE
//     → joins (HashJoin / NestedLoopJoin), left-deep        JOIN ... ON
//     → Aggregate → Filter                                  GROUP BY, HAVING
//     → Sort                                                ORDER BY
//     → Projection → Distinct                               SELECT [DISTINCT]
//     → Limit                                               LIMIT
// ---------------------------------------------------------------------------
class Planner {
public:
    explicit Planner(Database &db) : db_(db) {}

    // Returns the root operator; `column_names` receives the output column names.
    OperatorPtr plan_select(const SelectStmt &s, std::vector<std::string> &column_names);
    OperatorPtr plan_insert(const InsertStmt &s);
    OperatorPtr plan_update(const UpdateStmt &s);
    OperatorPtr plan_delete(const DeleteStmt &s);

private:
    // Scan one table, using an index if a conjunct allows it, then filter.
    OperatorPtr plan_table_access(const std::string &table, const std::string &alias,
                                  const std::vector<ExprPtr> &conjuncts);

    Database &db_;
};
