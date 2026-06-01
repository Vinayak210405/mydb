#pragma once
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include "../catalog/Type.h"
#include "../catalog/Column.h"

enum class ExprType { LITERAL, COLUMN_REF, BINARY_OP };

struct Expr {
    ExprType type;
    Value literal;
    std::string col_name;
    std::string op;
    std::shared_ptr<Expr> left;
    std::shared_ptr<Expr> right;
};

inline std::shared_ptr<Expr> make_literal(Value v) {
    auto e = std::make_shared<Expr>();
    e->type = ExprType::LITERAL; e->literal = std::move(v); return e;
}
inline std::shared_ptr<Expr> make_col_ref(const std::string &name) {
    auto e = std::make_shared<Expr>();
    e->type = ExprType::COLUMN_REF; e->col_name = name; return e;
}
inline std::shared_ptr<Expr> make_binop(std::string op,
                                         std::shared_ptr<Expr> l,
                                         std::shared_ptr<Expr> r) {
    auto e = std::make_shared<Expr>();
    e->type = ExprType::BINARY_OP; e->op = std::move(op);
    e->left = std::move(l); e->right = std::move(r); return e;
}

enum class StmtType { SELECT, INSERT, UPDATE, DELETE, CREATE_TABLE, DROP_TABLE, SHOW_TABLES, DESCRIBE };

struct SelectStmt {
    std::vector<std::string> columns;
    std::string              table;
    std::shared_ptr<Expr>    where;
};
struct InsertStmt {
    std::string              table;
    std::vector<std::string> columns;
    std::vector<Value>       values;
};
struct UpdateStmt {
    std::string                                      table;
    std::vector<std::pair<std::string,Value>>         assignments;
    std::shared_ptr<Expr>                            where;
};
struct DeleteStmt {
    std::string           table;
    std::shared_ptr<Expr> where;
};
struct CreateTableStmt {
    std::string         table;
    std::vector<Column> columns;
};
struct DropTableStmt  { std::string table; };
struct ShowTablesStmt {};
struct DescribeStmt   { std::string table; };

struct Statement {
    StmtType        type;
    SelectStmt      select;
    InsertStmt      insert;
    UpdateStmt      update;
    DeleteStmt      del;
    CreateTableStmt create_table;
    DropTableStmt   drop_table;
    DescribeStmt    describe;
};
