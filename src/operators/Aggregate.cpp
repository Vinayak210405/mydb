#include "Aggregate.h"
#include <stdexcept>

Aggregate::Aggregate(OperatorPtr child, const std::vector<ExprPtr> &group_by,
                     const std::vector<AggSpec> &aggs) {
    const Schema &in = child->schema();
    for (auto &g : group_by) {
        ExprPtr b = bind_expr(g, in);
        ColumnInfo ci;
        ci.name = g->to_string();      // the planner refers to group columns by this label
        ci.type = infer_type(*b, in);
        schema_.push_back(ci);
        group_by_.push_back(b);
    }
    for (auto a : aggs) {
        if (a.arg) a.arg = bind_expr(a.arg, in);
        ColumnInfo ci;
        ci.name = a.label;
        if (a.func == "COUNT")    ci.type = DataType::INT;
        else if (a.func == "AVG") ci.type = DataType::FLOAT;
        else                      ci.type = infer_type(*a.arg, in);
        schema_.push_back(ci);
        aggs_.push_back(a);
    }
    children_.push_back(std::move(child));
}

void Aggregate::open() {
    children_[0]->open();
    results_.clear();
    pos_ = 0;

    std::unordered_map<std::vector<Value>, size_t, RowHash, RowEqual> index;  // key → group #
    std::vector<std::vector<Value>> keys;
    std::vector<std::vector<State>> states;

    Row r;
    std::vector<Value> key;
    while (children_[0]->get_next(r)) {
        key.clear();
        for (auto &g : group_by_) key.push_back(eval_expr(*g, r.values));
        auto it = index.find(key);
        size_t gi;
        if (it == index.end()) {
            gi = keys.size();
            index.emplace(key, gi);
            keys.push_back(key);
            states.emplace_back(aggs_.size());
        } else {
            gi = it->second;
        }
        for (size_t i = 0; i < aggs_.size(); i++) {
            const AggSpec &a = aggs_[i];
            State &s = states[gi][i];
            if (!a.arg) { s.count++; continue; }               // COUNT(*)
            Value v = eval_expr(*a.arg, r.values);
            if (is_null(v)) continue;                            // aggregates skip NULLs
            if (a.distinct && !s.seen.emplace(std::vector<Value>{v}, true).second) continue;
            s.count++;
            if (a.func == "SUM" || a.func == "AVG") {
                if (!is_numeric(v)) throw std::runtime_error(a.func + " needs numbers");
                s.sum += as_double(v);
                if (a.func == "SUM") {
                    if (is_null(s.acc)) s.acc = v;
                    else if (is_int(s.acc) && is_int(v)) s.acc = std::get<int64_t>(s.acc) + std::get<int64_t>(v);
                    else s.acc = as_double(s.acc) + as_double(v);
                }
            } else if (a.func == "MIN") {
                if (is_null(s.acc) || compare_values(v, s.acc) < 0) s.acc = v;
            } else if (a.func == "MAX") {
                if (is_null(s.acc) || compare_values(v, s.acc) > 0) s.acc = v;
            }
        }
    }

    if (keys.empty() && group_by_.empty()) {                     // global aggregate on empty input
        keys.emplace_back();
        states.emplace_back(aggs_.size());
    }
    for (size_t gi = 0; gi < keys.size(); gi++) {
        Row out;
        out.values = keys[gi];
        for (size_t i = 0; i < aggs_.size(); i++) {
            const State &s = states[gi][i];
            const std::string &f = aggs_[i].func;
            if (f == "COUNT")    out.values.push_back(s.count);
            else if (f == "AVG") out.values.push_back(s.count ? Value{s.sum / s.count} : Value{});
            else                 out.values.push_back(s.acc);
        }
        results_.push_back(std::move(out));
    }
}

bool Aggregate::next(Row &out) {
    if (pos_ >= results_.size()) return false;
    out = results_[pos_++];
    return true;
}

std::string Aggregate::detail() const {
    std::string s;
    for (size_t i = 0; i < aggs_.size(); i++) {
        if (i) s += ", ";
        s += aggs_[i].label;
    }
    if (!group_by_.empty()) {
        s += " GROUP BY ";
        for (size_t i = 0; i < group_by_.size(); i++) {
            if (i) s += ", ";
            s += group_by_[i]->to_string();
        }
    }
    return s;
}
