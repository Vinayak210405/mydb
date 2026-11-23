#include "AST.h"

ExprPtr make_literal(Value v) {
    auto e = std::make_shared<Expr>();
    e->type = ExprType::LITERAL;
    e->literal = std::move(v);
    return e;
}

ExprPtr make_column(std::string table, std::string name) {
    auto e = std::make_shared<Expr>();
    e->type = ExprType::COLUMN;
    e->table = std::move(table);
    e->name = std::move(name);
    return e;
}

ExprPtr make_unary(std::string op, ExprPtr child) {
    auto e = std::make_shared<Expr>();
    e->type = ExprType::UNARY;
    e->op = std::move(op);
    e->args.push_back(std::move(child));
    return e;
}

ExprPtr make_binary(std::string op, ExprPtr l, ExprPtr r) {
    auto e = std::make_shared<Expr>();
    e->type = ExprType::BINARY;
    e->op = std::move(op);
    e->args.push_back(std::move(l));
    e->args.push_back(std::move(r));
    return e;
}

bool is_aggregate_name(const std::string &n) {
    return n == "COUNT" || n == "SUM" || n == "AVG" || n == "MIN" || n == "MAX";
}

bool contains_aggregate(const ExprPtr &e) {
    if (!e) return false;
    if (e->type == ExprType::FUNCTION && is_aggregate_name(e->name)) return true;
    for (auto &a : e->args)
        if (contains_aggregate(a)) return true;
    return false;
}

static std::string literal_sql(const Value &v) {
    if (is_string(v)) {
        std::string out = "'";
        for (char c : std::get<std::string>(v)) {
            if (c == '\'') out += "''";
            else out += c;
        }
        return out + "'";
    }
    return value_to_str(v);
}

std::string Expr::to_string() const {
    switch (type) {
        case ExprType::LITERAL:
            return literal_sql(literal);
        case ExprType::COLUMN:
            return table.empty() ? name : table + "." + name;
        case ExprType::UNARY:
            return op == "NOT" ? "NOT " + args[0]->to_string()
                               : op + args[0]->to_string();
        case ExprType::BINARY: {
            std::string o = op;
            if (op == "LIKE" && negated) o = "NOT LIKE";
            return "(" + args[0]->to_string() + " " + o + " " + args[1]->to_string() + ")";
        }
        case ExprType::IS_NULL:
            return args[0]->to_string() + (negated ? " IS NOT NULL" : " IS NULL");
        case ExprType::FUNCTION: {
            std::string s = name + "(";
            if (distinct) s += "DISTINCT ";
            if (star_arg) s += "*";
            else if (!args.empty()) s += args[0]->to_string();
            return s + ")";
        }
    }
    return "?";
}
