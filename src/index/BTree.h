#pragma once
#include <memory>
#include <vector>
#include <optional>
#include "BTreeNode.h"
#include "../storage/BufferPool.h"
#include "../storage/DiskManager.h"

class BTree {
public:
    BTree(std::shared_ptr<BufferPool>  pool,
          std::shared_ptr<DiskManager> disk);

    // Insert a key ? RID mapping
    void insert(int64_t key, RID rid);

    // Exact lookup — returns matching RIDs
    std::vector<RID> lookup(int64_t key) const;

    // Range scan [low, high] inclusive
    std::vector<RID> range(int64_t low, int64_t high) const;

    // Delete a key
    bool remove(int64_t key, RID rid);

private:
    PageID find_leaf(int64_t key) const;
    void   insert_in_leaf(PageID leaf_pid, int64_t key, RID rid);
    void   insert_in_parent(PageID left, int64_t key, PageID right);
    void   split_leaf(PageID leaf_pid, int64_t &out_key, PageID &out_right);
    void   split_internal(PageID node_pid, int64_t &out_key, PageID &out_right);

    BTreeNode load_node(PageID pid) const;
    void      save_node(PageID pid, BTreeNode &node);
    PageID    alloc_node(bool is_leaf);

    std::shared_ptr<BufferPool>  pool_;
    std::shared_ptr<DiskManager> disk_;
    PageID                       root_ = INVALID_PAGE_ID;
};
