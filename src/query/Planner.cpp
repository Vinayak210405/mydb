#include "Planner.h"
#include "Optimizer.h"
#include "Expression.h"
#include "../operators/SeqScan.h"
#include "../operators/IndexScan.h"
#include "../operators/Filter.h"
#include "../operators/Projection.h"
#include "../operators/NestedLoopJoin.h"
#include "../operators/HashJoin.h"
#include "../operators/Sort.h"
#include "../operators/Aggregate.h"
#include "../operators/Limit.h"
#include "../operators/Insert.h"
#include "../operators/Update.h"
#include "../operators/Delete.h"
#include <stdexcept>
#include <map>
#include <set>

using optimizer::TableInfo;

namespace {

// Produces a single empty row — the input for "SELECT 1 + 1" (no FROM).
class OneRow : public Operator {
public:
    void open() override { done_ = false; }
    std::string name() const override { return "OneRow"; }
protected:
    bool next(Row &out) override {
        if (done_) return false;
        done_ = true;
        out.values.clear();
        out.rid = RID{};
        return true;
    }
private:
    bool done_ = false;
};

// Canonical text of a BOUND expression: columns appear as their slot number,
// so "name" and "c.name" referring to the same column get the same key.
std::string bound_key(const Expr &e) {
    switch (e.type) {
        case ExprType::COLUMN:  return "$" + std::to_string(e.slot);
        case ExprType::LITERAL: return e.to_string();
        case ExprType::UNARY:   return e.op + "(" + bound_key(*e.args[0]) + ")";
        case ExprType::BINARY:  return "(" + bound_key(*e.args[0]) + " " + e.op +
                                       (e.negated ? "!" : "") + " " + bound_key(*e.args[1]) + ")";
        case ExprType::IS_NULL: return bound_key(*e.args[0]) + (e.negated ? " NOTNULL" : " ISNULL");
        case ExprType::FUNCTION: break;
    }
    return "?";
}

// Plans GROUP BY / aggregates: finds every aggregate call, builds the
// Aggregate operator, and rewrites the expressions above it so they read the
// aggregate's output columns instead.
class AggregationPlan {
public:
    AggregationPlan(const Schema &input, const std::vector<ExprPtr> &group_by) : in_(input) {
        for (auto &g : group_by) {
            if (contains_aggregate(g)) throw std::runtime_error("aggregates are not allowed in GROUP BY");
            std::string k = bound_key(*bind_expr(g, in_));
            if (group_index_.count(k)) continue;
            group_index_[k] = group_.size();
            group_.push_back(g);
        }
    }

    void collect(const ExprPtr &e) {
        if (!e) return;
        if (e->type == ExprType::FUNCTION) {
            std::string k = agg_key(*e);
            if (!agg_index_.count(k)) {
                for (auto &a : e->args)
                    if (contains_aggregate(a)) throw std::runtime_error("aggregates cannot be nested");
                agg_index_[k] = aggs_.size();
                AggSpec spec;
                spec.func = e->name;
                spec.arg = e->star_arg ? nullptr : e->args[0];
                spec.distinct = e->distinct;
                spec.label = e->to_string();
                aggs_.push_back(spec);
            }
            return;
        }
        for (auto &a : e->args) collect(a);
    }

    OperatorPtr build(OperatorPtr child) {
        return std::make_unique<Aggregate>(std::move(child), group_, aggs_);
    }

    // Rewrite an expression to run on the Aggregate's output.
    ExprPtr rewrite(const ExprPtr &e) const {
        if (e->type == ExprType::FUNCTION)
            return make_column("", aggs_[agg_index_.at(agg_key(*e))].label);
        if (!contains_aggregate(e)) {
            auto it = group_index_.find(bound_key(*bind_expr(e, in_)));
            if (it != group_index_.end()) return make_column("", group_[it->second]->to_string());
            if (e->type == ExprType::COLUMN)
                throw std::runtime_error("column '" + e->to_string() +
                                         "' must appear in GROUP BY or be used in an aggregate function");
            if (e->type == ExprType::LITERAL) return e;
        }
        auto copy = std::make_shared<Expr>(*e);
        for (auto &a : copy->args) a = rewrite(a);
        return copy;
    }

private:
    std::string agg_key(const Expr &e) const {
        std::string k = e.name + "(" + (e.distinct ? "DISTINCT " : "");
        k += e.star_arg ? "*" : bound_key(*bind_expr(e.args[0], in_));
        return k + ")";
    }

    Schema                        in_;
    std::vector<ExprPtr>          group_;
    std::map<std::string, size_t> group_index_;
    std::vector<AggSpec>          aggs_;
    std::map<std::string, size_t> agg_index_;
};

}  // namespace

// ---------------------------------------------------------------------------
OperatorPtr Planner::plan_table_access(const std::string &table, const std::string &alias,
                                       const std::vector<ExprPtr> &conjuncts) {
    const TableDef *def = db_.catalog().get_table(table);
    if (!def) throw std::runtime_error("table '" + table + "' not found");
    auto path = optimizer::choose_access_path({alias, def}, conjuncts);
    OperatorPtr op;
    if (path.use_index)
        op = std::make_unique<IndexScan>(db_, table, alias, path.column, path.low, path.high);
    else
        op = std::make_unique<SeqScan>(db_, table, alias);
    if (!path.remaining.empty())
        op = std::make_unique<Filter>(std::move(op), join_conjuncts(path.remaining));
    return op;
}

OperatorPtr Planner::plan_select(const SelectStmt &s, std::vector<std::string> &column_names) {
    // ---- 1. FROM: resolve tables ------------------------------------------------
    std::vector<TableInfo> tables;
    for (auto &ref : s.from) {
        const TableDef *def = db_.catalog().get_table(ref.name);
        if (!def) throw std::runtime_error("table '" + ref.name + "' not found");
        for (auto &t : tables)
            if (t.alias == ref.alias)
                throw std::runtime_error("table name/alias '" + ref.alias + "' is used twice; give one an alias");
        tables.push_back({ref.alias, def});
    }

    // ---- 2. WHERE + ON → conjuncts, each tagged with the tables it needs ----------
    std::vector<ExprPtr> all;
    split_conjuncts(s.where, all);
    for (auto &ref : s.from) split_conjuncts(ref.join_on, all);
    for (auto &c : all)
        if (contains_aggregate(c))
            throw std::runtime_error("aggregates are not allowed in WHERE or ON (use HAVING)");

    std::map<std::string, std::vector<ExprPtr>> single;     // alias → its own conjuncts
    std::vector<std::pair<ExprPtr, std::set<std::string>>> multi;
    std::vector<ExprPtr> constant;
    for (auto &c : all) {
        auto aliases = optimizer::referenced_aliases(c, tables);
        if (aliases.empty())          constant.push_back(c);
        else if (aliases.size() == 1) single[*aliases.begin()].push_back(c);
        else                          multi.emplace_back(c, aliases);
    }

    // ---- 3. scans + joins (left-deep, in FROM order) ----------------------------
    OperatorPtr current;
    if (tables.empty()) {
        current = std::make_unique<OneRow>();
    } else {
        current = plan_table_access(s.from[0].name, tables[0].alias, single[tables[0].alias]);
        std::set<std::string> available{tables[0].alias};
        for (size_t i = 1; i < tables.size(); i++) {
            const std::string &alias = tables[i].alias;
            OperatorPtr right = plan_table_access(s.from[i].name, alias, single[alias]);

            std::vector<ExprPtr> left_keys, right_keys, residual;
            for (auto it = multi.begin(); it != multi.end();) {
                bool ready = it->second.count(alias) > 0;
                for (auto &a : it->second)
                    if (a != alias && !available.count(a)) ready = false;
                if (!ready) { ++it; continue; }
                ExprPtr lk, rk;
                if (optimizer::as_equi_join(it->first, tables, available, alias, lk, rk)) {
                    left_keys.push_back(lk);
                    right_keys.push_back(rk);
                } else {
                    residual.push_back(it->first);
                }
                it = multi.erase(it);
            }
            if (!left_keys.empty())
                current = std::make_unique<HashJoin>(std::move(current), std::move(right),
                                                     left_keys, right_keys, join_conjuncts(residual));
            else
                current = std::make_unique<NestedLoopJoin>(std::move(current), std::move(right),
                                                           join_conjuncts(residual));
            available.insert(alias);
        }
    }
    for (auto &[c, _] : multi) constant.push_back(c);         // (shouldn't happen)
    if (!constant.empty())
        current = std::make_unique<Filter>(std::move(current), join_conjuncts(constant));

    // ---- 4. expand the SELECT list ---------------------------------------------
    struct Item { ExprPtr expr; std::string name; };
    std::vector<Item> items;
    bool has_star = false;
    for (auto &it : s.items) {
        if (it.star) {
            has_star = true;
            bool any = false;
            for (auto &col : current->schema()) {
                if (!it.star_table.empty() && col.table != it.star_table) continue;
                items.push_back({make_column(col.table, col.name), col.name});
                any = true;
            }
            if (!any) throw std::runtime_error("unknown table '" + it.star_table + "' in " + it.star_table + ".*");
            continue;
        }
        std::string name = it.alias;
        if (name.empty()) name = it.expr->type == ExprType::COLUMN ? it.expr->name : it.expr->to_string();
        items.push_back({it.expr, name});
    }

    // ORDER BY may name a select alias or a 1-based position.
    std::vector<ExprPtr> order_exprs;
    std::vector<bool>    order_desc;
    for (auto &o : s.order_by) {
        ExprPtr e = o.expr;
        if (e->type == ExprType::LITERAL && is_int(e->literal)) {
            int64_t k = std::get<int64_t>(e->literal);
            if (k < 1 || k > (int64_t)items.size())
                throw std::runtime_error("ORDER BY position " + std::to_string(k) + " is out of range");
            e = items[k - 1].expr;
        } else if (e->type == ExprType::COLUMN && e->table.empty()) {
            for (size_t i = 0; i < s.items.size(); i++)
                if (!s.items[i].star && s.items[i].alias == e->name) e = s.items[i].expr;
        }
        order_exprs.push_back(e);
        order_desc.push_back(o.desc);
    }

    // ---- 5. GROUP BY / aggregates / HAVING ---------------------------------------
    bool has_agg = !s.group_by.empty() || contains_aggregate(s.having);
    for (auto &it : items) has_agg = has_agg || contains_aggregate(it.expr);
    for (auto &e : order_exprs) has_agg = has_agg || contains_aggregate(e);
    if (s.having && !has_agg) throw std::runtime_error("HAVING needs GROUP BY or an aggregate");

    if (has_agg) {
        if (has_star) throw std::runtime_error("SELECT * cannot be combined with GROUP BY or aggregates");
        AggregationPlan agg(current->schema(), s.group_by);
        for (auto &it : items) agg.collect(it.expr);
        agg.collect(s.having);
        for (auto &e : order_exprs) agg.collect(e);
        current = agg.build(std::move(current));
        if (s.having) current = std::make_unique<Filter>(std::move(current), agg.rewrite(s.having));
        for (auto &it : items) it.expr = agg.rewrite(it.expr);
        for (auto &e : order_exprs) e = agg.rewrite(e);
    }

    // ---- 6. ORDER BY → SELECT list → DISTINCT → LIMIT ----------------------------
    if (!order_exprs.empty())
        current = std::make_unique<Sort>(std::move(current), order_exprs, order_desc);

    std::vector<ExprPtr> proj;
    column_names.clear();
    for (auto &it : items) {
        proj.push_back(it.expr);
        column_names.push_back(it.name);
    }
    current = std::make_unique<Projection>(std::move(current), proj, column_names);
    if (s.distinct) current = std::make_unique<Distinct>(std::move(current));   // keeps sorted order
    if (s.limit >= 0) current = std::make_unique<Limit>(std::move(current), s.limit);
    return current;
}

OperatorPtr Planner::plan_insert(const InsertStmt &s) {
    const TableDef *def = db_.catalog().get_table(s.table);
    if (!def) throw std::runtime_error("table '" + s.table + "' not found");

    std::vector<int> targets;                       // value position → column index
    if (s.columns.empty()) {
        for (int i = 0; i < (int)def->columns.size(); i++) targets.push_back(i);
    } else {
        std::set<int> seen;
        for (auto &c : s.columns) {
            int idx = def->column_index(c);
            if (idx < 0) throw std::runtime_error("unknown column '" + c + "' in table '" + s.table + "'");
            if (!seen.insert(idx).second) throw std::runtime_error("column '" + c + "' listed twice");
            targets.push_back(idx);
        }
    }
    std::vector<std::vector<Value>> rows;
    for (auto &exprs : s.rows) {
        if (exprs.size() != targets.size())
            throw std::runtime_error("INSERT has " + std::to_string(exprs.size()) + " values but " +
                                     std::to_string(targets.size()) + " columns");
        std::vector<Value> row(def->columns.size(), std::monostate{});
        for (size_t i = 0; i < exprs.size(); i++) {
            ExprPtr b;
            try { b = bind_expr(exprs[i], Schema{}); }
            catch (std::exception &) { throw std::runtime_error("INSERT values must be constants: " + exprs[i]->to_string()); }
            row[targets[i]] = eval_expr(*b, {});
        }
        rows.push_back(std::move(row));
    }
    return std::make_unique<Insert>(db_, s.table, std::move(rows));
}

OperatorPtr Planner::plan_update(const UpdateStmt &s) {
    const TableDef *def = db_.catalog().get_table(s.table);
    if (!def) throw std::runtime_error("table '" + s.table + "' not found");
    std::vector<ExprPtr> conj;
    split_conjuncts(s.where, conj);
    for (auto &c : conj) optimizer::referenced_aliases(c, {{s.table, def}});   // validates columns
    OperatorPtr scan = plan_table_access(s.table, s.table, conj);
    return std::make_unique<Update>(db_, s.table, std::move(scan), s.assignments);
}

OperatorPtr Planner::plan_delete(const DeleteStmt &s) {
    const TableDef *def = db_.catalog().get_table(s.table);
    if (!def) throw std::runtime_error("table '" + s.table + "' not found");
    std::vector<ExprPtr> conj;
    split_conjuncts(s.where, conj);
    for (auto &c : conj) optimizer::referenced_aliases(c, {{s.table, def}});
    OperatorPtr scan = plan_table_access(s.table, s.table, conj);
    return std::make_unique<Delete>(db_, s.table, std::move(scan));
}
