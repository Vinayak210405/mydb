#pragma once
#include <string>
#include <vector>
#include <optional>
#include <mutex>
#include "AST.h"
#include "Executor.h"
#include "../engine/Database.h"

struct QueryResult {
    bool        ok = true;
    std::string sql;              // the statement this result belongs to
    std::string kind;             // select, insert, update, delete, ddl, show, describe, explain
    std::string message;          // "3 rows inserted", or the error text
    std::vector<std::string>        columns;
    std::vector<std::string>        column_types;
    std::vector<std::vector<Value>> rows;
    int64_t                 rows_affected = -1;
    std::optional<PlanNode> plan;            // the operator tree that ran
    double                  elapsed_ms = 0;

    static QueryResult error(const std::string &msg, const std::string &sql = "");
    std::string to_string() const;           // aligned text table (terminal client)
};

// ---------------------------------------------------------------------------
// QueryEngine — the front door: SQL text in, results out.
//
//   SQL ──Lexer──▶ tokens ──Parser──▶ AST ──Planner──▶ operator tree ──Executor──▶ rows
//
// Statements run one at a time (a single engine-wide lock): there are no
// transactions yet, so this is what keeps concurrent clients from
// interleaving writes.
// ---------------------------------------------------------------------------
class QueryEngine {
public:
    explicit QueryEngine(Database &db) : db_(db) {}

    // Run every statement in `sql`; stops at the first error (which is the last result).
    std::vector<QueryResult> execute_script(const std::string &sql);
    // Run a single statement (or a script, returning the last result).
    QueryResult execute(const std::string &sql);

    // Run `fn` while holding the engine lock (for read-only introspection).
    template <typename Fn>
    auto with_lock(Fn fn) {
        std::lock_guard<std::mutex> lock(mutex_);
        return fn(db_);
    }

private:
    QueryResult run_statement(const Statement &stmt);
    QueryResult run_select(const SelectStmt &s);
    QueryResult run_dml(OperatorPtr plan, const std::string &kind, const std::string &verb);
    QueryResult run_explain(const Statement &inner);

    Database   &db_;
    std::mutex  mutex_;
};
