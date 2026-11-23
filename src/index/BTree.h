#pragma once
#include <memory>
#include <string>
#include <vector>
#include "BTreeNode.h"
#include "../storage/BufferPool.h"

// ---------------------------------------------------------------------------
// BTree — a disk-based B+ tree mapping int64 keys to RIDs. Duplicate keys
// are allowed (each (key, RID) pair is a separate entry).
//
// File layout: page 0 is a meta page that records the root page id, so the
// tree can be reopened after a restart. Every other page is a node.
//
// Deletion removes the entry from its leaf but does not merge under-full
// nodes ("lazy deletion"); the tree stays correct, it just never shrinks.
// ---------------------------------------------------------------------------
class BTree {
public:
    explicit BTree(const std::string &path, size_t pool_frames = 64);

    void             insert(int64_t key, RID rid);
    bool             remove(int64_t key, RID rid);
    std::vector<RID> lookup(int64_t key);
    std::vector<RID> range(int64_t low, int64_t high);   // inclusive

    // --- introspection (used by tests and the web console's tree view) ---
    struct NodeInfo {
        PageID               page;
        bool                 leaf;
        std::vector<int64_t> keys;
        std::vector<PageID>  children;   // internal nodes only
        PageID               next_leaf;
    };
    // Nodes grouped by level, root first. Stops after `max_nodes` nodes.
    std::vector<std::vector<NodeInfo>> levels(size_t max_nodes = 400);
    int    height();
    size_t entry_count();
    PageID root() const { return root_; }
    size_t node_count() { return disk_->page_count() > 0 ? disk_->page_count() - 1 : 0; }

    BufferPool&  pool() { return *pool_; }
    DiskManager& disk() { return *disk_; }
    void         flush() { pool_->flush_all(); }

private:
    PageID find_leaf_for_search(int64_t key);
    PageID alloc_node(bool leaf);
    void   set_root(PageID pid);

    std::shared_ptr<DiskManager> disk_;
    std::unique_ptr<BufferPool>  pool_;
    PageID                       root_ = INVALID_PAGE_ID;
};
