#pragma once
#include <string>
#include <vector>
#include <set>
#include <climits>
#include "AST.h"
#include "../catalog/TableDef.h"

// ---------------------------------------------------------------------------
// Rule-based optimizer helpers used by the Planner.
//
//  1. Predicate pushdown — split WHERE/ON into AND-ed conjuncts and apply each
//     one as low in the plan as possible (right above the table it needs).
//  2. Access path selection — if a table's conjuncts compare an indexed INT
//     column with a constant (=, <, <=, >, >=, BETWEEN), read the table through
//     its B+ tree (IndexScan) instead of scanning every page (SeqScan).
//  3. Join algorithm selection — equality joins use a HashJoin; anything else
//     falls back to a NestedLoopJoin.
// ---------------------------------------------------------------------------
namespace optimizer {

// One table in the FROM clause, as seen by the optimizer.
struct TableInfo {
    std::string     alias;
    const TableDef *def;
};

// Which table aliases does this expression reference? Unqualified column
// names are resolved against the tables in scope (error if unknown/ambiguous).
std::set<std::string> referenced_aliases(const ExprPtr &e, const std::vector<TableInfo> &tables);

struct AccessPath {
    bool                 use_index = false;
    std::string          column;
    int64_t              low  = INT64_MIN;
    int64_t              high = INT64_MAX;
    std::vector<ExprPtr> remaining;        // conjuncts the index does not cover
};

// Pick SeqScan vs IndexScan for one table given the conjuncts that only
// reference that table.
AccessPath choose_access_path(const TableInfo &table, const std::vector<ExprPtr> &conjuncts);

// If `e` is "left_col = right_col" with one side from `left_aliases` and the
// other from `right_alias`, return the two sides (left side first).
bool as_equi_join(const ExprPtr &e, const std::vector<TableInfo> &tables,
                  const std::set<std::string> &left_aliases, const std::string &right_alias,
                  ExprPtr &left_key, ExprPtr &right_key);

}  // namespace optimizer
