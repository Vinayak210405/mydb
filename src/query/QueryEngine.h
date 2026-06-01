#pragma once
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include "../catalog/Catalog.h"
#include "../storage/HeapFile.h"
#include "../storage/BufferPool.h"
#include "../storage/DiskManager.h"
#include "../storage/Tuple.h"
#include "../index/IndexManager.h"

struct QueryResult {
    bool        ok      = true;
    std::string message;
    std::vector<std::string>        columns;
    std::vector<std::vector<Value>> rows;
    std::string to_string() const;
};

class QueryEngine {
public:
    QueryEngine(std::shared_ptr<Catalog>     catalog,
                std::shared_ptr<BufferPool>  pool,
                std::shared_ptr<DiskManager> disk,
                std::shared_ptr<BufferPool>  idx_pool,
                std::shared_ptr<DiskManager> idx_disk);

    QueryResult execute(const std::string &sql);

private:
    QueryResult exec_create_table(const struct CreateTableStmt &s);
    QueryResult exec_drop_table  (const struct DropTableStmt   &s);
    QueryResult exec_show_tables ();
    QueryResult exec_describe    (const struct DescribeStmt    &s);
    QueryResult exec_insert      (const struct InsertStmt      &s);
    QueryResult exec_select      (const struct SelectStmt      &s);
    QueryResult exec_delete      (const struct DeleteStmt      &s);
    QueryResult exec_update      (const struct UpdateStmt      &s);
    QueryResult exec_create_index(const std::string &table, const std::string &column);

    bool  eval_where(const struct Expr *expr,
                     const std::vector<Value> &row,
                     const struct TableDef    &def) const;
    Value eval_expr (const struct Expr *expr,
                     const std::vector<Value> &row,
                     const struct TableDef    &def) const;

    HeapFile& get_heap(const std::string &table_name);

    std::shared_ptr<Catalog>      catalog_;
    std::shared_ptr<BufferPool>   pool_;
    std::shared_ptr<DiskManager>  disk_;
    std::shared_ptr<IndexManager> idx_mgr_;
    std::unordered_map<std::string, std::shared_ptr<HeapFile>> heaps_;
};
