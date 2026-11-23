#include "Filter.h"

Filter::Filter(OperatorPtr child, ExprPtr predicate) {
    schema_ = child->schema();
    predicate_ = bind_expr(predicate, schema_);
    children_.push_back(std::move(child));
}

bool Filter::next(Row &out) {
    while (children_[0]->get_next(out)) {
        if (is_true(eval_expr(*predicate_, out.values))) return true;
    }
    return false;
}
