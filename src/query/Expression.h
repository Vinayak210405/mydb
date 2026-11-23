#pragma once
#include <string>
#include <vector>
#include "AST.h"
#include "../catalog/Type.h"

// ---------------------------------------------------------------------------
// Row schemas and expression evaluation.
//
// Before an expression runs against rows, it is BOUND to the schema of those
// rows: every column reference is resolved once to a slot index, so
// evaluating it per row is just an array lookup.
// ---------------------------------------------------------------------------
struct ColumnInfo {
    std::string table;   // table alias the column came from ("" for computed columns)
    std::string name;
    DataType    type = DataType::NULLTYPE;
};
using Schema = std::vector<ColumnInfo>;

// Find a column in a schema. Returns -1 if absent; throws if ambiguous.
int resolve_column(const Schema &schema, const std::string &table, const std::string &name);

// Returns a copy of `e` with column slots filled in. Throws on unknown or
// ambiguous columns, and on aggregate functions (those must be planned by
// the Aggregate operator first).
ExprPtr bind_expr(const ExprPtr &e, const Schema &schema);

// Evaluate a bound expression against one row.
Value eval_expr(const Expr &e, const std::vector<Value> &row);

// SQL WHERE semantics: only TRUE passes (FALSE and NULL do not).
bool is_true(const Value &v);

// Best-effort static type of a bound expression (for result column types).
DataType infer_type(const Expr &e, const Schema &schema);

// SQL LIKE with % (any run) and _ (one character).
bool like_match(const std::string &text, const std::string &pattern);

// Split "a AND b AND c" into [a, b, c].
void split_conjuncts(const ExprPtr &e, std::vector<ExprPtr> &out);
ExprPtr join_conjuncts(const std::vector<ExprPtr> &parts);

// Every (table, name) column referenced by an expression.
void collect_columns(const ExprPtr &e, std::vector<const Expr*> &out);
