#include "Expression.h"
#include <stdexcept>
#include <cmath>

int resolve_column(const Schema &schema, const std::string &table, const std::string &name) {
    int found = -1;
    for (int i = 0; i < (int)schema.size(); i++) {
        if (schema[i].name != name) continue;
        if (!table.empty() && schema[i].table != table) continue;
        if (found >= 0)
            throw std::runtime_error("column '" + name + "' is ambiguous; qualify it as table." + name);
        found = i;
    }
    return found;
}

ExprPtr bind_expr(const ExprPtr &e, const Schema &schema) {
    if (!e) return nullptr;
    auto out = std::make_shared<Expr>(*e);
    switch (e->type) {
        case ExprType::COLUMN: {
            int idx = resolve_column(schema, e->table, e->name);
            if (idx < 0) {
                std::string full = e->table.empty() ? e->name : e->table + "." + e->name;
                throw std::runtime_error("unknown column '" + full + "'");
            }
            out->slot = idx;
            return out;
        }
        case ExprType::FUNCTION:
            throw std::runtime_error("aggregate " + e->to_string() + " is not allowed here");
        default:
            break;
    }
    for (auto &a : out->args) a = bind_expr(a, schema);
    return out;
}

bool is_true(const Value &v) {
    return is_bool(v) && std::get<bool>(v);
}

bool like_match(const std::string &text, const std::string &pat) {
    // Iterative wildcard match with backtracking on the last '%'.
    size_t t = 0, p = 0, star = std::string::npos, mark = 0;
    while (t < text.size()) {
        if (p < pat.size() && (pat[p] == '_' || pat[p] == text[t])) { t++; p++; }
        else if (p < pat.size() && pat[p] == '%') { star = p++; mark = t; }
        else if (star != std::string::npos) { p = star + 1; t = ++mark; }
        else return false;
    }
    while (p < pat.size() && pat[p] == '%') p++;
    return p == pat.size();
}

static Value arith(const std::string &op, const Value &l, const Value &r) {
    if (is_null(l) || is_null(r)) return std::monostate{};
    if (!is_numeric(l) || !is_numeric(r))
        throw std::runtime_error(std::string("operator ") + op + " needs numbers, got " +
                                 value_type_name(l) + " and " + value_type_name(r));
    if (is_int(l) && is_int(r)) {
        int64_t a = std::get<int64_t>(l), b = std::get<int64_t>(r);
        if (op == "+") return a + b;
        if (op == "-") return a - b;
        if (op == "*") return a * b;
        if (b == 0) throw std::runtime_error("division by zero");
        if (op == "/") return a / b;
        return a % b;
    }
    double a = as_double(l), b = as_double(r);
    if (op == "+") return a + b;
    if (op == "-") return a - b;
    if (op == "*") return a * b;
    if (b == 0) throw std::runtime_error("division by zero");
    if (op == "/") return a / b;
    return std::fmod(a, b);
}

Value eval_expr(const Expr &e, const std::vector<Value> &row) {
    switch (e.type) {
        case ExprType::LITERAL:
            return e.literal;
        case ExprType::COLUMN:
            if (e.slot < 0 || e.slot >= (int)row.size())
                throw std::runtime_error("internal error: unbound column " + e.to_string());
            return row[e.slot];
        case ExprType::IS_NULL: {
            bool n = is_null(eval_expr(*e.args[0], row));
            return e.negated ? !n : n;
        }
        case ExprType::UNARY: {
            Value v = eval_expr(*e.args[0], row);
            if (is_null(v)) return v;
            if (e.op == "NOT") {
                if (!is_bool(v)) throw std::runtime_error("NOT needs a boolean");
                return !std::get<bool>(v);
            }
            if (is_int(v))   return -std::get<int64_t>(v);
            if (is_float(v)) return -std::get<double>(v);
            throw std::runtime_error("unary - needs a number");
        }
        case ExprType::BINARY: {
            const std::string &op = e.op;
            if (op == "AND" || op == "OR") {
                // SQL three-valued logic
                Value l = eval_expr(*e.args[0], row);
                if (!is_null(l) && !is_bool(l)) throw std::runtime_error(op + " needs booleans");
                if (op == "AND" && is_bool(l) && !std::get<bool>(l)) return false;
                if (op == "OR"  && is_bool(l) &&  std::get<bool>(l)) return true;
                Value r = eval_expr(*e.args[1], row);
                if (!is_null(r) && !is_bool(r)) throw std::runtime_error(op + " needs booleans");
                if (op == "AND") {
                    if (is_bool(r) && !std::get<bool>(r)) return false;
                    if (is_null(l) || is_null(r)) return std::monostate{};
                    return true;
                }
                if (is_bool(r) && std::get<bool>(r)) return true;
                if (is_null(l) || is_null(r)) return std::monostate{};
                return false;
            }
            Value l = eval_expr(*e.args[0], row);
            Value r = eval_expr(*e.args[1], row);
            if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%")
                return arith(op, l, r);
            if (is_null(l) || is_null(r)) return std::monostate{};
            if (op == "LIKE") {
                if (!is_string(l) || !is_string(r)) throw std::runtime_error("LIKE needs strings");
                bool m = like_match(std::get<std::string>(l), std::get<std::string>(r));
                return e.negated ? !m : m;
            }
            int c = compare_values(l, r);
            if (op == "=")  return c == 0;
            if (op == "!=") return c != 0;
            if (op == "<")  return c < 0;
            if (op == ">")  return c > 0;
            if (op == "<=") return c <= 0;
            if (op == ">=") return c >= 0;
            throw std::runtime_error("unknown operator " + op);
        }
        case ExprType::FUNCTION:
            throw std::runtime_error("aggregate " + e.to_string() + " is not allowed here");
    }
    return std::monostate{};
}

DataType infer_type(const Expr &e, const Schema &schema) {
    switch (e.type) {
        case ExprType::LITERAL:
            if (is_int(e.literal))    return DataType::INT;
            if (is_float(e.literal))  return DataType::FLOAT;
            if (is_string(e.literal)) return DataType::VARCHAR;
            if (is_bool(e.literal))   return DataType::BOOL;
            return DataType::NULLTYPE;
        case ExprType::COLUMN:
            return (e.slot >= 0 && e.slot < (int)schema.size()) ? schema[e.slot].type : DataType::NULLTYPE;
        case ExprType::IS_NULL:
            return DataType::BOOL;
        case ExprType::UNARY:
            return e.op == "NOT" ? DataType::BOOL : infer_type(*e.args[0], schema);
        case ExprType::BINARY: {
            const std::string &op = e.op;
            if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%") {
                DataType a = infer_type(*e.args[0], schema), b = infer_type(*e.args[1], schema);
                return (a == DataType::INT && b == DataType::INT) ? DataType::INT : DataType::FLOAT;
            }
            return DataType::BOOL;
        }
        case ExprType::FUNCTION:
            if (e.name == "COUNT") return DataType::INT;
            if (e.name == "AVG")   return DataType::FLOAT;
            return e.args.empty() ? DataType::NULLTYPE : infer_type(*e.args[0], schema);
    }
    return DataType::NULLTYPE;
}

void split_conjuncts(const ExprPtr &e, std::vector<ExprPtr> &out) {
    if (!e) return;
    if (e->type == ExprType::BINARY && e->op == "AND") {
        split_conjuncts(e->args[0], out);
        split_conjuncts(e->args[1], out);
    } else {
        out.push_back(e);
    }
}

ExprPtr join_conjuncts(const std::vector<ExprPtr> &parts) {
    ExprPtr out;
    for (auto &p : parts) out = out ? make_binary("AND", out, p) : p;
    return out;
}

void collect_columns(const ExprPtr &e, std::vector<const Expr*> &out) {
    if (!e) return;
    if (e->type == ExprType::COLUMN) out.push_back(e.get());
    for (auto &a : e->args) collect_columns(a, out);
}
