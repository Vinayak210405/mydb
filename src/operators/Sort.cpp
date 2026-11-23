#include "Sort.h"
#include <algorithm>

Sort::Sort(OperatorPtr child, const std::vector<ExprPtr> &keys, const std::vector<bool> &desc)
    : desc_(desc) {
    schema_ = child->schema();
    for (auto &k : keys) keys_.push_back(bind_expr(k, schema_));
    children_.push_back(std::move(child));
}

void Sort::open() {
    children_[0]->open();
    rows_.clear();
    pos_ = 0;

    // Evaluate each sort key once per row, then sort the (keys, row) pairs.
    struct Item { std::vector<Value> keys; Row row; };
    std::vector<Item> items;
    Row r;
    while (children_[0]->get_next(r)) {
        Item it;
        for (auto &k : keys_) it.keys.push_back(eval_expr(*k, r.values));
        it.row = r;
        items.push_back(std::move(it));
    }
    std::stable_sort(items.begin(), items.end(), [&](const Item &a, const Item &b) {
        for (size_t i = 0; i < keys_.size(); i++) {
            const Value &x = a.keys[i], &y = b.keys[i];
            int c;
            if (is_null(x) || is_null(y)) c = is_null(x) ? (is_null(y) ? 0 : -1) : 1;
            else c = compare_values(x, y);
            if (c != 0) return desc_[i] ? c > 0 : c < 0;
        }
        return false;
    });
    rows_.reserve(items.size());
    for (auto &it : items) rows_.push_back(std::move(it.row));
}

bool Sort::next(Row &out) {
    if (pos_ >= rows_.size()) return false;
    out = rows_[pos_++];
    return true;
}

std::string Sort::detail() const {
    std::string s;
    for (size_t i = 0; i < keys_.size(); i++) {
        if (i) s += ", ";
        s += keys_[i]->to_string() + (desc_[i] ? " DESC" : " ASC");
    }
    return s;
}
