#include "Limit.h"

size_t RowHash::operator()(const std::vector<Value> &v) const {
    size_t h = 1469598103934665603ULL;
    for (auto &x : v) h = (h ^ hash_value(x)) * 1099511628211ULL;
    return h;
}

bool RowEqual::operator()(const std::vector<Value> &a, const std::vector<Value> &b) const {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++)
        if (!values_equal(a[i], b[i])) return false;
    return true;
}

size_t Distinct::Hash::operator()(const std::vector<Value> &v) const { return RowHash{}(v); }
bool Distinct::Equal::operator()(const std::vector<Value> &a, const std::vector<Value> &b) const {
    return RowEqual{}(a, b);
}

bool Distinct::next(Row &out) {
    while (children_[0]->get_next(out)) {
        if (seen_.insert(out.values).second) return true;
    }
    return false;
}
