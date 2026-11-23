#include "Projection.h"

Projection::Projection(OperatorPtr child, const std::vector<ExprPtr> &exprs,
                       const std::vector<std::string> &names) {
    const Schema &in = child->schema();
    for (size_t i = 0; i < exprs.size(); i++) {
        ExprPtr b = bind_expr(exprs[i], in);
        ColumnInfo ci;
        ci.name = names[i];
        ci.type = infer_type(*b, in);
        if (b->type == ExprType::COLUMN) ci.table = in[b->slot].table;
        schema_.push_back(ci);
        exprs_.push_back(b);
    }
    children_.push_back(std::move(child));
}

bool Projection::next(Row &out) {
    if (!children_[0]->get_next(in_)) return false;
    out.values.clear();
    out.values.reserve(exprs_.size());
    for (auto &e : exprs_) out.values.push_back(eval_expr(*e, in_.values));
    out.rid = in_.rid;
    return true;
}

std::string Projection::detail() const {
    std::string s;
    for (size_t i = 0; i < exprs_.size(); i++) {
        if (i) s += ", ";
        s += exprs_[i]->to_string();
    }
    return s;
}
