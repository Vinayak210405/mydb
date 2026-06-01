#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include "BTree.h"
#include "../storage/BufferPool.h"
#include "../storage/DiskManager.h"

class IndexManager {
public:
    IndexManager(std::shared_ptr<BufferPool>  pool,
                 std::shared_ptr<DiskManager> disk);

    // Create an index — key is "table.column"
    void create_index(const std::string &table, const std::string &column);

    // Get index if exists
    BTree* get_index(const std::string &table, const std::string &column);

    bool has_index(const std::string &table, const std::string &column) const;

private:
    std::shared_ptr<BufferPool>                        pool_;
    std::shared_ptr<DiskManager>                       disk_;
    std::unordered_map<std::string, std::shared_ptr<BTree>> indexes_;
};
