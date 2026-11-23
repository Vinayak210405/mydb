#include "Optimizer.h"
#include "Expression.h"
#include <stdexcept>
#include <map>
#include <algorithm>

namespace optimizer {

static std::string owner_alias(const Expr &col, const std::vector<TableInfo> &tables) {
    if (!col.table.empty()) {
        for (auto &t : tables)
            if (t.alias == col.table) {
                if (t.def->column_index(col.name) < 0)
                    throw std::runtime_error("unknown column '" + col.table + "." + col.name + "'");
                return t.alias;
            }
        throw std::runtime_error("unknown table or alias '" + col.table + "'");
    }
    std::string found;
    for (auto &t : tables) {
        if (t.def->column_index(col.name) < 0) continue;
        if (!found.empty())
            throw std::runtime_error("column '" + col.name + "' is ambiguous; qualify it as table." + col.name);
        found = t.alias;
    }
    if (found.empty()) throw std::runtime_error("unknown column '" + col.name + "'");
    return found;
}

std::set<std::string> referenced_aliases(const ExprPtr &e, const std::vector<TableInfo> &tables) {
    std::vector<const Expr*> cols;
    collect_columns(e, cols);
    std::set<std::string> out;
    for (auto *c : cols) out.insert(owner_alias(*c, tables));
    return out;
}

// Is `e` a comparison between a column of this table and an integer constant?
// Normalises "5 < id" to "id > 5".
static bool as_bound(const ExprPtr &e, const TableInfo &t,
                     std::string &col, std::string &op, int64_t &val) {
    if (e->type != ExprType::BINARY) return false;
    static const std::map<std::string, std::string> flip = {
        {"=", "="}, {"<", ">"}, {">", "<"}, {"<=", ">="}, {">=", "<="}};
    auto f = flip.find(e->op);
    if (f == flip.end()) return false;
    const ExprPtr &a = e->args[0], &b = e->args[1];
    const Expr *column = nullptr, *lit = nullptr;
    bool flipped = false;
    if (a->type == ExprType::COLUMN && b->type == ExprType::LITERAL) { column = a.get(); lit = b.get(); }
    else if (b->type == ExprType::COLUMN && a->type == ExprType::LITERAL) { column = b.get(); lit = a.get(); flipped = true; }
    else return false;
    if (!is_int(lit->literal)) return false;
    if (!column->table.empty() && column->table != t.alias) return false;
    int ci = t.def->column_index(column->name);
    if (ci < 0 || t.def->columns[ci].type != DataType::INT) return false;
    col = column->name;
    op  = flipped ? f->second : e->op;
    val = std::get<int64_t>(lit->literal);
    return true;
}

AccessPath choose_access_path(const TableInfo &t, const std::vector<ExprPtr> &conjuncts) {
    // Gather [low, high] bounds per indexed column.
    struct Bounds { int64_t low = INT64_MIN, high = INT64_MAX; bool eq = false; std::vector<size_t> used; };
    std::map<std::string, Bounds> per_col;
    for (size_t i = 0; i < conjuncts.size(); i++) {
        std::string col, op;
        int64_t v;
        if (!as_bound(conjuncts[i], t, col, op, v) || !t.def->has_index(col)) continue;
        Bounds &b = per_col[col];
        if (op == "=")       { b.low = std::max(b.low, v); b.high = std::min(b.high, v); b.eq = true; }
        else if (op == ">=") { b.low = std::max(b.low, v); }
        else if (op == ">")  { b.low = std::max(b.low, v == INT64_MAX ? v : v + 1); if (v == INT64_MAX) b.high = INT64_MIN; }
        else if (op == "<=") { b.high = std::min(b.high, v); }
        else if (op == "<")  { b.high = std::min(b.high, v == INT64_MIN ? v : v - 1); if (v == INT64_MIN) b.low = INT64_MAX; }
        b.used.push_back(i);
    }

    AccessPath path;
    const Bounds *best = nullptr;
    for (auto &[col, b] : per_col) {
        // Prefer an equality lookup; otherwise the first usable range.
        if (!best || (b.eq && !best->eq)) { best = &b; path.column = col; }
    }
    if (best) {
        path.use_index = true;
        path.low  = best->low;
        path.high = best->high;
    }
    for (size_t i = 0; i < conjuncts.size(); i++) {
        bool covered = best && std::find(best->used.begin(), best->used.end(), i) != best->used.end();
        if (!covered) path.remaining.push_back(conjuncts[i]);
    }
    return path;
}

bool as_equi_join(const ExprPtr &e, const std::vector<TableInfo> &tables,
                  const std::set<std::string> &left_aliases, const std::string &right_alias,
                  ExprPtr &left_key, ExprPtr &right_key) {
    if (e->type != ExprType::BINARY || e->op != "=") return false;
    const ExprPtr &a = e->args[0], &b = e->args[1];
    if (a->type != ExprType::COLUMN || b->type != ExprType::COLUMN) return false;
    std::string ta = owner_alias(*a, tables), tb = owner_alias(*b, tables);
    if (left_aliases.count(ta) && tb == right_alias) { left_key = a; right_key = b; return true; }
    if (left_aliases.count(tb) && ta == right_alias) { left_key = b; right_key = a; return true; }
    return false;
}

}  // namespace optimizer
