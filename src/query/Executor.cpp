#include "Executor.h"

std::vector<std::vector<Value>> Executor::run(Operator &root) {
    std::vector<std::vector<Value>> rows;
    root.open();
    Row r;
    while (root.get_next(r)) rows.push_back(std::move(r.values));
    root.close();
    return rows;
}

PlanNode Executor::describe(const Operator &root, bool executed) {
    PlanNode n;
    n.name   = root.name();
    n.detail = root.detail();
    n.rows   = executed ? static_cast<int64_t>(root.rows_out()) : -1;
    for (auto *c : root.children()) n.children.push_back(describe(*c, executed));
    return n;
}

static void lines_rec(const PlanNode &n, const std::string &prefix, bool root, bool last,
                      std::vector<std::string> &out) {
    std::string line = root ? "" : prefix + (last ? "└─ " : "├─ ");
    line += n.name;
    if (!n.detail.empty()) line += "  " + n.detail;
    if (n.rows >= 0) line += "  [rows=" + std::to_string(n.rows) + "]";
    out.push_back(line);
    std::string child_prefix = root ? "" : prefix + (last ? "   " : "│  ");
    for (size_t i = 0; i < n.children.size(); i++)
        lines_rec(n.children[i], child_prefix, false, i + 1 == n.children.size(), out);
}

std::vector<std::string> Executor::to_lines(const PlanNode &node) {
    std::vector<std::string> out;
    lines_rec(node, "", true, true, out);
    return out;
}
