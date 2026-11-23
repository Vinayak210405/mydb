#pragma once
#include <string>
#include <vector>
#include "../operators/Operator.h"

// A snapshot of an operator tree, for EXPLAIN and for the web console's plan view.
struct PlanNode {
    std::string           name;
    std::string           detail;
    int64_t               rows = -1;   // rows produced (-1 = not executed)
    std::vector<PlanNode> children;
};

class Executor {
public:
    // Pull every row out of the plan (open → next… → close).
    static std::vector<std::vector<Value>> run(Operator &root);

    // Describe the operator tree. Pass executed=true after run() to include row counts.
    static PlanNode describe(const Operator &root, bool executed);

    // Indented text form, one operator per line.
    static std::vector<std::string> to_lines(const PlanNode &node);
};
