#include "QueryEngine.h"
#include "Parser.h"
#include "Planner.h"
#include <sstream>
#include <chrono>
#include <algorithm>

// ---------------------------------------------------------------------------
// QueryResult
// ---------------------------------------------------------------------------
// Display width of a UTF-8 string (counts code points, not bytes).
static size_t display_width(const std::string &s) {
    size_t n = 0;
    for (unsigned char c : s) if ((c & 0xC0) != 0x80) n++;
    return n;
}

QueryResult QueryResult::error(const std::string &msg, const std::string &sql) {
    QueryResult r;
    r.ok = false;
    r.kind = "error";
    r.message = msg;
    r.sql = sql;
    return r;
}

std::string QueryResult::to_string() const {
    if (!ok) return "ERROR: " + message;
    if (columns.empty()) return message;

    std::vector<size_t> width(columns.size());
    for (size_t i = 0; i < columns.size(); i++) width[i] = display_width(columns[i]);
    for (auto &row : rows)
        for (size_t i = 0; i < row.size() && i < width.size(); i++)
            width[i] = std::max(width[i], display_width(value_to_str(row[i])));

    std::ostringstream ss;
    auto sep = [&]() {
        ss << "+";
        for (auto w : width) ss << std::string(w + 2, '-') << "+";
        ss << "\n";
    };
    auto line = [&](const std::vector<std::string> &cells) {
        ss << "|";
        for (size_t i = 0; i < cells.size(); i++)
            ss << " " << cells[i] << std::string(width[i] - display_width(cells[i]), ' ') << " |";
        ss << "\n";
    };
    sep();
    line(columns);
    sep();
    for (auto &row : rows) {
        std::vector<std::string> cells;
        for (auto &v : row) cells.push_back(value_to_str(v));
        line(cells);
    }
    sep();
    ss << rows.size() << (rows.size() == 1 ? " row" : " rows");
    return ss.str();
}

// ---------------------------------------------------------------------------
// Execution
// ---------------------------------------------------------------------------
std::vector<QueryResult> QueryEngine::execute_script(const std::string &sql) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<QueryResult> results;
    std::vector<Statement> stmts;
    try {
        stmts = Parser::parse_sql(sql);
    } catch (std::exception &e) {
        results.push_back(QueryResult::error(e.what(), sql));
        return results;
    }
    if (stmts.empty()) {
        results.push_back(QueryResult::error("empty query", sql));
        return results;
    }
    for (auto &stmt : stmts) {
        auto start = std::chrono::steady_clock::now();
        QueryResult r;
        try {
            r = run_statement(stmt);
        } catch (std::exception &e) {
            r = QueryResult::error(e.what());
        }
        try { db_.flush_all(); } catch (std::exception &e) {
            if (r.ok) r = QueryResult::error(std::string("flush failed: ") + e.what());
        }
        r.sql = stmt.sql;
        r.elapsed_ms = std::chrono::duration<double, std::milli>(
                           std::chrono::steady_clock::now() - start).count();
        bool failed = !r.ok;
        results.push_back(std::move(r));
        if (failed) break;
    }
    return results;
}

QueryResult QueryEngine::execute(const std::string &sql) {
    auto results = execute_script(sql);
    return results.back();
}

static QueryResult message_result(const std::string &kind, const std::string &msg) {
    QueryResult r;
    r.kind = kind;
    r.message = msg;
    return r;
}

QueryResult QueryEngine::run_statement(const Statement &stmt) {
    Planner planner(db_);
    switch (stmt.type) {
        case StmtType::SELECT:
            return run_select(stmt.select);
        case StmtType::INSERT:
            return run_dml(planner.plan_insert(stmt.insert), "insert", "inserted");
        case StmtType::UPDATE:
            return run_dml(planner.plan_update(stmt.update), "update", "updated");
        case StmtType::DELETE:
            return run_dml(planner.plan_delete(stmt.del), "delete", "deleted");
        case StmtType::EXPLAIN:
            return run_explain(*stmt.explained);

        case StmtType::CREATE_TABLE: {
            const auto &s = stmt.create_table;
            if (db_.catalog().table_exists(s.table)) {
                if (s.if_not_exists) return message_result("ddl", "table '" + s.table + "' already exists (skipped)");
                throw std::runtime_error("table '" + s.table + "' already exists");
            }
            TableDef def(s.table);
            for (auto &c : s.columns) def.add_column(c);
            db_.create_table(def);
            return message_result("ddl", "table '" + s.table + "' created");
        }
        case StmtType::DROP_TABLE: {
            const auto &s = stmt.drop_table;
            if (!db_.catalog().table_exists(s.table)) {
                if (s.if_exists) return message_result("ddl", "table '" + s.table + "' does not exist (skipped)");
                throw std::runtime_error("table '" + s.table + "' not found");
            }
            db_.drop_table(s.table);
            return message_result("ddl", "table '" + s.table + "' dropped");
        }
        case StmtType::CREATE_INDEX:
            db_.create_index(stmt.index.table, stmt.index.column);
            return message_result("ddl", "index created on " + stmt.index.table + "." + stmt.index.column);
        case StmtType::DROP_INDEX:
            db_.drop_index(stmt.index.table, stmt.index.column);
            return message_result("ddl", "index on " + stmt.index.table + "." + stmt.index.column + " dropped");

        case StmtType::SHOW_TABLES: {
            QueryResult r;
            r.kind = "show";
            r.columns = {"table_name", "rows", "indexes"};
            r.column_types = {"VARCHAR", "INT", "VARCHAR"};
            for (auto &name : db_.catalog().list_tables()) {
                const TableDef *def = db_.catalog().get_table(name);
                std::string idx;
                for (auto &c : def->indexed_columns) idx += (idx.empty() ? "" : ", ") + c;
                r.rows.push_back({name, static_cast<int64_t>(db_.heap(name).row_count()), idx});
            }
            return r;
        }
        case StmtType::DESCRIBE: {
            const TableDef *def = db_.catalog().get_table(stmt.describe.table);
            if (!def) throw std::runtime_error("table '" + stmt.describe.table + "' not found");
            QueryResult r;
            r.kind = "describe";
            r.columns = {"column", "type", "nullable", "primary_key", "indexed"};
            r.column_types = {"VARCHAR", "VARCHAR", "BOOL", "BOOL", "BOOL"};
            for (auto &c : def->columns)
                r.rows.push_back({c.name, std::string(dtype_str(c.type)), c.nullable,
                                  c.is_primary_key, def->has_index(c.name)});
            return r;
        }
    }
    throw std::runtime_error("unsupported statement");
}

QueryResult QueryEngine::run_select(const SelectStmt &s) {
    Planner planner(db_);
    QueryResult r;
    r.kind = "select";
    OperatorPtr plan = planner.plan_select(s, r.columns);
    for (auto &c : plan->schema()) r.column_types.push_back(dtype_str(c.type));
    r.rows = Executor::run(*plan);
    r.plan = Executor::describe(*plan, true);
    r.message = std::to_string(r.rows.size()) + (r.rows.size() == 1 ? " row" : " rows");
    return r;
}

QueryResult QueryEngine::run_dml(OperatorPtr plan, const std::string &kind, const std::string &verb) {
    auto rows = Executor::run(*plan);
    QueryResult r;
    r.kind = kind;
    r.rows_affected = rows.empty() ? 0 : std::get<int64_t>(rows[0][0]);
    r.plan = Executor::describe(*plan, true);
    r.message = std::to_string(r.rows_affected) + (r.rows_affected == 1 ? " row " : " rows ") + verb;
    return r;
}

QueryResult QueryEngine::run_explain(const Statement &inner) {
    Planner planner(db_);
    OperatorPtr plan;
    std::vector<std::string> cols;
    switch (inner.type) {
        case StmtType::SELECT: plan = planner.plan_select(inner.select, cols); break;
        case StmtType::UPDATE: plan = planner.plan_update(inner.update); break;
        case StmtType::DELETE: plan = planner.plan_delete(inner.del); break;
        default: throw std::runtime_error("EXPLAIN supports SELECT, UPDATE and DELETE");
    }
    QueryResult r;
    r.kind = "explain";
    r.plan = Executor::describe(*plan, false);
    r.columns = {"plan"};
    r.column_types = {"VARCHAR"};
    for (auto &line : Executor::to_lines(*r.plan)) r.rows.push_back({line});
    r.message = "plan only (not executed)";
    return r;
}
