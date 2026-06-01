#include "QueryEngine.h"
#include "Lexer.h"
#include "Parser.h"
#include "AST.h"
#include <sstream>
#include <stdexcept>
#include <algorithm>

std::string QueryResult::to_string() const {
    if (!ok) return "ERROR: " + message;
    if (!message.empty()) return message;
    if (rows.empty()) return "(0 rows)";
    std::ostringstream ss;
    for (size_t i = 0; i < columns.size(); i++) {
        ss << columns[i];
        if (i + 1 < columns.size()) ss << " | ";
    }
    ss << "\n";
    for (auto &row : rows) {
        for (size_t i = 0; i < row.size(); i++) {
            ss << value_to_str(row[i]);
            if (i + 1 < row.size()) ss << " | ";
        }
        ss << "\n";
    }
    return ss.str();
}

QueryEngine::QueryEngine(std::shared_ptr<Catalog> catalog,
                         std::shared_ptr<BufferPool> pool,
                         std::shared_ptr<DiskManager> disk,
                         std::shared_ptr<BufferPool> idx_pool,
                         std::shared_ptr<DiskManager> idx_disk)
    : catalog_(std::move(catalog))
    , pool_(std::move(pool))
    , disk_(std::move(disk))
    , idx_mgr_(std::make_shared<IndexManager>(idx_pool, idx_disk)) {}

HeapFile& QueryEngine::get_heap(const std::string &name) {
    auto it = heaps_.find(name);
    if (it != heaps_.end()) return *it->second;
    heaps_[name] = std::make_shared<HeapFile>(name, pool_, disk_);
    return *heaps_[name];
}

QueryResult QueryEngine::execute(const std::string &sql) {
    try {
        std::string up = sql;
        std::transform(up.begin(), up.end(), up.begin(), ::toupper);
        if (up.rfind("CREATE INDEX", 0) == 0) {
            size_t on_pos = up.find(" ON ");
            size_t lp_pos = sql.find(40);
            size_t rp_pos = sql.find(41);
            if (on_pos == std::string::npos || lp_pos == std::string::npos)
                return {false, "syntax: CREATE INDEX ON table (column)"};
            std::string table  = sql.substr(on_pos + 4, lp_pos - on_pos - 4);
            std::string column = sql.substr(lp_pos + 1, rp_pos - lp_pos - 1);
            auto trim = [](std::string s) {
                size_t a = s.find_first_not_of(" \t");
                size_t b = s.find_last_not_of(" \t");
                return (a == std::string::npos) ? "" : s.substr(a, b-a+1);
            };
            return exec_create_index(trim(table), trim(column));
        }
        Lexer lexer(sql);
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        Statement stmt = parser.parse();
        switch (stmt.type) {
            case StmtType::CREATE_TABLE: return exec_create_table(stmt.create_table);
            case StmtType::DROP_TABLE:   return exec_drop_table(stmt.drop_table);
            case StmtType::SHOW_TABLES:  return exec_show_tables();
            case StmtType::DESCRIBE:     return exec_describe(stmt.describe);
            case StmtType::INSERT:       return exec_insert(stmt.insert);
            case StmtType::SELECT:       return exec_select(stmt.select);
            case StmtType::DELETE:       return exec_delete(stmt.del);
            case StmtType::UPDATE:       return exec_update(stmt.update);
            default: return {false, "unsupported statement"};
        }
    } catch (std::exception &e) {
        return {false, e.what()};
    }
}

QueryResult QueryEngine::exec_create_table(const CreateTableStmt &s) {
    TableDef def(s.table);
    for (auto &c : s.columns) def.add_column(c);
    if (!catalog_->create_table(std::move(def)))
        return {false, "table '" + s.table + "' already exists"};
    return {true, "OK: table '" + s.table + "' created"};
}

QueryResult QueryEngine::exec_drop_table(const DropTableStmt &s) {
    if (!catalog_->drop_table(s.table))
        return {false, "table '" + s.table + "' not found"};
    heaps_.erase(s.table);
    return {true, "OK: table '" + s.table + "' dropped"};
}

QueryResult QueryEngine::exec_show_tables() {
    auto tables = catalog_->list_tables();
    QueryResult r;
    r.columns = {"table_name"};
    for (auto &t : tables) r.rows.push_back({t});
    return r;
}

QueryResult QueryEngine::exec_describe(const DescribeStmt &s) {
    const TableDef *def = catalog_->get_table(s.table);
    if (!def) return {false, "table '" + s.table + "' not found"};
    QueryResult r;
    r.columns = {"column", "type", "nullable", "primary_key"};
    for (auto &c : def->columns)
        r.rows.push_back({c.name, std::string(dtype_str(c.type)),
                          std::string(c.nullable ? "YES" : "NO"),
                          std::string(c.is_primary_key ? "YES" : "NO")});
    return r;
}

QueryResult QueryEngine::exec_create_index(const std::string &table, const std::string &column) {
    if (!catalog_->table_exists(table))
        return {false, "table '" + table + "' not found"};
    const TableDef *def = catalog_->get_table(table);
    int col_idx = def->column_index(column);
    if (col_idx < 0) return {false, "column '" + column + "' not found"};
    if (def->columns[col_idx].type != DataType::INT)
        return {false, "indexes only supported on INT columns for now"};
    idx_mgr_->create_index(table, column);
    std::vector<DataType> types;
    for (auto &c : def->columns) types.push_back(c.type);
    HeapFile &heap = get_heap(table);
    BTree *idx = idx_mgr_->get_index(table, column);
    heap.scan([&](RID rid, const char *data, uint16_t len) {
        auto row = Tuple::deserialize(data, len, types);
        if (col_idx < (int)row.size() && std::holds_alternative<int64_t>(row[col_idx]))
            idx->insert(std::get<int64_t>(row[col_idx]), rid);
    });
    return {true, "OK: index created on " + table + "." + column};
}

Value QueryEngine::eval_expr(const Expr *expr, const std::vector<Value> &row, const TableDef &def) const {
    if (expr->type == ExprType::LITERAL) return expr->literal;
    if (expr->type == ExprType::COLUMN_REF) {
        int idx = def.column_index(expr->col_name);
        if (idx < 0 || idx >= (int)row.size()) return std::monostate{};
        return row[idx];
    }
    if (expr->type == ExprType::BINARY_OP) {
        Value l = eval_expr(expr->left.get(), row, def);
        Value r = eval_expr(expr->right.get(), row, def);
        bool l_int = std::holds_alternative<int64_t>(l);
        bool r_int = std::holds_alternative<int64_t>(r);
        if (l_int && r_int) {
            int64_t lv = std::get<int64_t>(l), rv = std::get<int64_t>(r);
            if (expr->op == "=")  return (bool)(lv == rv);
            if (expr->op == "!=") return (bool)(lv != rv);
            if (expr->op == "<")  return (bool)(lv <  rv);
            if (expr->op == ">")  return (bool)(lv >  rv);
            if (expr->op == "<=") return (bool)(lv <= rv);
            if (expr->op == ">=") return (bool)(lv >= rv);
        }
        std::string ls = value_to_str(l), rs = value_to_str(r);
        if (expr->op == "=")  return (bool)(ls == rs);
        if (expr->op == "!=") return (bool)(ls != rs);
        if (expr->op == "<")  return (bool)(ls <  rs);
        if (expr->op == ">")  return (bool)(ls >  rs);
        if (expr->op == "<=") return (bool)(ls <= rs);
        if (expr->op == ">=") return (bool)(ls >= rs);
    }
    return std::monostate{};
}

bool QueryEngine::eval_where(const Expr *expr, const std::vector<Value> &row, const TableDef &def) const {
    if (!expr) return true;
    Value result = eval_expr(expr, row, def);
    if (std::holds_alternative<bool>(result)) return std::get<bool>(result);
    return false;
}

QueryResult QueryEngine::exec_insert(const InsertStmt &s) {
    const TableDef *def = catalog_->get_table(s.table);
    if (!def) return {false, "table '" + s.table + "' not found"};
    std::vector<Value> row(def->columns.size(), std::monostate{});
    if (s.columns.empty()) {
        if (s.values.size() != def->columns.size())
            return {false, "value count does not match column count"};
        row = s.values;
    } else {
        if (s.values.size() != s.columns.size())
            return {false, "column/value count mismatch"};
        for (size_t i = 0; i < s.columns.size(); i++) {
            int idx = def->column_index(s.columns[i]);
            if (idx < 0) return {false, "unknown column '" + s.columns[i] + "'"};
            row[idx] = s.values[i];
        }
    }
    auto bytes = Tuple::serialize(row);
    HeapFile &heap = get_heap(s.table);
    RID rid = heap.insert(bytes.data(), static_cast<uint16_t>(bytes.size()));
    for (size_t i = 0; i < def->columns.size(); i++) {
        if (def->columns[i].type == DataType::INT) {
            BTree *idx = idx_mgr_->get_index(s.table, def->columns[i].name);
            if (idx && std::holds_alternative<int64_t>(row[i]))
                idx->insert(std::get<int64_t>(row[i]), rid);
        }
    }
    return {true, "OK: 1 row inserted"};
}

QueryResult QueryEngine::exec_select(const SelectStmt &s) {
    const TableDef *def = catalog_->get_table(s.table);
    if (!def) return {false, "table '" + s.table + "' not found"};
    std::vector<int> col_indices;
    std::vector<std::string> col_names;
    if (s.columns.empty()) {
        for (int i = 0; i < (int)def->columns.size(); i++) {
            col_indices.push_back(i);
            col_names.push_back(def->columns[i].name);
        }
    } else {
        for (auto &c : s.columns) {
            int idx = def->column_index(c);
            if (idx < 0) return {false, "unknown column '" + c + "'"};
            col_indices.push_back(idx);
            col_names.push_back(c);
        }
    }
    std::vector<DataType> types;
    for (auto &c : def->columns) types.push_back(c.type);
    QueryResult result;
    result.columns = col_names;
    HeapFile &heap = get_heap(s.table);
    // Try index lookup
    if (s.where && s.where->type == ExprType::BINARY_OP && s.where->op == "=") {
        Expr *left  = s.where->left.get();
        Expr *right = s.where->right.get();
        if (left->type == ExprType::COLUMN_REF &&
            right->type == ExprType::LITERAL &&
            std::holds_alternative<int64_t>(right->literal)) {
            BTree *idx = idx_mgr_->get_index(s.table, left->col_name);
            if (idx) {
                int64_t val = std::get<int64_t>(right->literal);
                auto rids = idx->lookup(val);
                char buf[PAGE_SIZE];
                for (auto &rid : rids) {
                    uint16_t len = heap.fetch(rid, buf, PAGE_SIZE);
                    if (len == 0) continue;
                    auto row = Tuple::deserialize(buf, len, types);
                    if (!eval_where(s.where.get(), row, *def)) continue;
                    std::vector<Value> projected;
                    for (int i : col_indices)
                        projected.push_back(i < (int)row.size() ? row[i] : std::monostate{});
                    result.rows.push_back(std::move(projected));
                }
                return result;
            }
        }
    }
    // Full scan fallback
    heap.scan([&](RID, const char *data, uint16_t len) {
        auto row = Tuple::deserialize(data, len, types);
        if (!eval_where(s.where.get(), row, *def)) return;
        std::vector<Value> projected;
        for (int idx : col_indices)
            projected.push_back(idx < (int)row.size() ? row[idx] : std::monostate{});
        result.rows.push_back(std::move(projected));
    });
    return result;
}

QueryResult QueryEngine::exec_delete(const DeleteStmt &s) {
    const TableDef *def = catalog_->get_table(s.table);
    if (!def) return {false, "table '" + s.table + "' not found"};
    std::vector<DataType> types;
    for (auto &c : def->columns) types.push_back(c.type);
    std::vector<RID> to_delete;
    HeapFile &heap = get_heap(s.table);
    heap.scan([&](RID rid, const char *data, uint16_t len) {
        auto row = Tuple::deserialize(data, len, types);
        if (eval_where(s.where.get(), row, *def))
            to_delete.push_back(rid);
    });
    for (auto &rid : to_delete) heap.remove(rid);
    return {true, "OK: " + std::to_string(to_delete.size()) + " row(s) deleted"};
}

QueryResult QueryEngine::exec_update(const UpdateStmt &s) {
    const TableDef *def = catalog_->get_table(s.table);
    if (!def) return {false, "table '" + s.table + "' not found"};
    std::vector<DataType> types;
    for (auto &c : def->columns) types.push_back(c.type);
    std::vector<std::pair<RID, std::vector<Value>>> to_update;
    HeapFile &heap = get_heap(s.table);
    heap.scan([&](RID rid, const char *data, uint16_t len) {
        auto row = Tuple::deserialize(data, len, types);
        if (!eval_where(s.where.get(), row, *def)) return;
        for (auto &[col, val] : s.assignments) {
            int idx = def->column_index(col);
            if (idx >= 0 && idx < (int)row.size()) row[idx] = val;
        }
        to_update.emplace_back(rid, std::move(row));
    });
    int count = 0;
    for (auto &[rid, row] : to_update) {
        heap.remove(rid);
        auto bytes = Tuple::serialize(row);
        heap.insert(bytes.data(), static_cast<uint16_t>(bytes.size()));
        count++;
    }
    return {true, "OK: " + std::to_string(count) + " row(s) updated"};
}