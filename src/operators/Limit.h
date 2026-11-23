#pragma once
#include <unordered_set>
#include "Operator.h"

// Stops after N rows — and, because operators pull rows on demand, the
// operators below it stop working too.
class Limit : public Operator {
public:
    Limit(OperatorPtr child, int64_t n) : n_(n) {
        schema_ = child->schema();
        children_.push_back(std::move(child));
    }
    void open() override  { seen_ = 0; children_[0]->open(); }
    void close() override { children_[0]->close(); }
    std::string name() const override { return "Limit"; }
    std::string detail() const override { return std::to_string(n_); }

protected:
    bool next(Row &out) override {
        if (seen_ >= n_) return false;
        if (!children_[0]->get_next(out)) return false;
        seen_++;
        return true;
    }

private:
    int64_t n_;
    int64_t seen_ = 0;
};

// SELECT DISTINCT: drops rows already produced.
class Distinct : public Operator {
public:
    explicit Distinct(OperatorPtr child) {
        schema_ = child->schema();
        children_.push_back(std::move(child));
    }
    void open() override  { seen_.clear(); children_[0]->open(); }
    void close() override { children_[0]->close(); }
    std::string name() const override { return "Distinct"; }

protected:
    bool next(Row &out) override;

private:
    struct Hash  { size_t operator()(const std::vector<Value> &v) const; };
    struct Equal { bool operator()(const std::vector<Value> &a, const std::vector<Value> &b) const; };
    std::unordered_set<std::vector<Value>, Hash, Equal> seen_;
};

// Shared helpers for hashing a row of values (used by Distinct, HashJoin, Aggregate).
struct RowHash  { size_t operator()(const std::vector<Value> &v) const; };
struct RowEqual { bool operator()(const std::vector<Value> &a, const std::vector<Value> &b) const; };
