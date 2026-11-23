#pragma once
#include <string>
#include <vector>
#include <memory>
#include "../catalog/Type.h"
#include "../catalog/Column.h"

// ---------------------------------------------------------------------------
// Expressions
// ---------------------------------------------------------------------------
enum class ExprType {
    LITERAL,      // 42, 'abc', TRUE, NULL
    COLUMN,       // name or table.name
    UNARY,        // op: "-" or "NOT"
    BINARY,       // op: = != < > <= >= + - * / % AND OR LIKE
    IS_NULL,      // child IS [NOT] NULL   (negated = IS NOT NULL)
    FUNCTION,     // COUNT/SUM/AVG/MIN/MAX(...)  — aggregate functions
};

struct Expr;
using ExprPtr = std::shared_ptr<Expr>;

struct Expr {
    ExprType             type = ExprType::LITERAL;
    Value                literal;
    std::string          table;        // COLUMN: optional qualifier
    std::string          name;         // COLUMN: column name, FUNCTION: upper-case name
    std::string          op;           // UNARY / BINARY operator
    std::vector<ExprPtr> args;         // UNARY: 1, BINARY: 2, IS_NULL: 1, FUNCTION: 0..1
    bool                 negated  = false;   // IS NOT NULL / NOT LIKE
    bool                 star_arg = false;   // COUNT(*)
    bool                 distinct = false;   // COUNT(DISTINCT x)
    int                  slot     = -1;      // COLUMN after binding: index in the input row

    std::string to_string() const;           // canonical SQL text (used as a key, and in EXPLAIN)
};

ExprPtr make_literal(Value v);
ExprPtr make_column(std::string table, std::string name);
ExprPtr make_unary(std::string op, ExprPtr child);
ExprPtr make_binary(std::string op, ExprPtr l, ExprPtr r);

bool is_aggregate_name(const std::string &upper_name);
bool contains_aggregate(const ExprPtr &e);

// ---------------------------------------------------------------------------
// Statements
// ---------------------------------------------------------------------------
struct SelectItem {
    ExprPtr     expr;          // null when star
    std::string alias;
    bool        star = false;  // * or t.*
    std::string star_table;
};

struct TableRef {
    std::string name;
    std::string alias;         // defaults to name
    ExprPtr     join_on;       // ON condition (null for the first table / cross joins)
};

struct OrderItem {
    ExprPtr expr;
    bool    desc = false;
};

struct SelectStmt {
    bool                    distinct = false;
    std::vector<SelectItem> items;
    std::vector<TableRef>   from;      // from[0] is the first table; the rest are joins
    ExprPtr                 where;
    std::vector<ExprPtr>    group_by;
    ExprPtr                 having;
    std::vector<OrderItem>  order_by;
    int64_t                 limit = -1;
};

struct InsertStmt {
    std::string                       table;
    std::vector<std::string>          columns;
    std::vector<std::vector<ExprPtr>> rows;
};

struct UpdateStmt {
    std::string                                  table;
    std::vector<std::pair<std::string, ExprPtr>> assignments;
    ExprPtr                                      where;
};

struct DeleteStmt {
    std::string table;
    ExprPtr     where;
};

struct CreateTableStmt {
    std::string         table;
    std::vector<Column> columns;
    bool                if_not_exists = false;
};

struct DropTableStmt   { std::string table; bool if_exists = false; };
struct IndexStmt       { std::string table; std::string column; };
struct DescribeStmt    { std::string table; };

enum class StmtType {
    SELECT, INSERT, UPDATE, DELETE,
    CREATE_TABLE, DROP_TABLE, CREATE_INDEX, DROP_INDEX,
    SHOW_TABLES, DESCRIBE, EXPLAIN
};

struct Statement {
    StmtType        type = StmtType::SELECT;
    std::string     sql;            // the text of this statement
    SelectStmt      select;
    InsertStmt      insert;
    UpdateStmt      update;
    DeleteStmt      del;
    CreateTableStmt create_table;
    DropTableStmt   drop_table;
    IndexStmt       index;
    DescribeStmt    describe;
    std::shared_ptr<Statement> explained;   // EXPLAIN <statement>
};
