#include "BTree.h"
#include <stdexcept>
#include <cstring>

// ---------------------------------------------------------------
// Low-level node I/O
// ---------------------------------------------------------------
BTreeNode BTree::load_node(PageID pid) const {
    BTreeNode node;
    Frame *f = pool_->fetch_page(pid);
    std::memcpy(node.raw(), f->page.raw(), PAGE_SIZE);
    pool_->unpin(pid, false);
    return node;
}

void BTree::save_node(PageID pid, BTreeNode &node) {
    Frame *f = pool_->fetch_page(pid);
    std::memcpy(f->page.raw(), node.raw(), PAGE_SIZE);
    pool_->unpin(pid, true);
}

PageID BTree::alloc_node(bool is_leaf) {
    PageID pid;
    Frame *f = pool_->new_page(pid);
    BTreeNode node;
    node.header()->is_leaf = is_leaf ? 1 : 0;
    node.header()->num_keys = 0;
    std::memcpy(f->page.raw(), node.raw(), PAGE_SIZE);
    pool_->unpin(pid, true);
    return pid;
}

// ---------------------------------------------------------------
// Constructor — create root leaf if needed
// ---------------------------------------------------------------
BTree::BTree(std::shared_ptr<BufferPool>  pool,
             std::shared_ptr<DiskManager> disk)
    : pool_(std::move(pool)), disk_(std::move(disk)) {
    if (disk_->page_count() == 0)
        root_ = alloc_node(true);
    else
        root_ = 0;
}

// ---------------------------------------------------------------
// Find the leaf page for a given key
// ---------------------------------------------------------------
PageID BTree::find_leaf(int64_t key) const {
    PageID cur = root_;
    while (true) {
        BTreeNode node = load_node(cur);
        if (node.is_leaf()) return cur;
        uint16_t n = node.num_keys();
        uint16_t i = 0;
        while (i < n && key >= node.get_key(i)) i++;
        cur = node.get_child(i);
    }
}

// ---------------------------------------------------------------
// Lookup
// ---------------------------------------------------------------
std::vector<RID> BTree::lookup(int64_t key) const {
    std::vector<RID> results;
    PageID pid = find_leaf(key);
    while (pid != INVALID_PAGE_ID) {
        BTreeNode node = load_node(pid);
        bool found = false;
        for (uint16_t i = 0; i < node.num_keys(); i++) {
            if (node.get_key(i) == key) {
                results.push_back(node.get_rid(i));
                found = true;
            } else if (node.get_key(i) > key) {
                break;
            }
        }
        if (!found) break;
        pid = node.header()->next_leaf;
    }
    return results;
}

// ---------------------------------------------------------------
// Range scan
// ---------------------------------------------------------------
std::vector<RID> BTree::range(int64_t low, int64_t high) const {
    std::vector<RID> results;
    PageID pid = find_leaf(low);
    while (pid != INVALID_PAGE_ID) {
        BTreeNode node = load_node(pid);
        bool past_high = false;
        for (uint16_t i = 0; i < node.num_keys(); i++) {
            int64_t k = node.get_key(i);
            if (k > high) { past_high = true; break; }
            if (k >= low)  results.push_back(node.get_rid(i));
        }
        if (past_high) break;
        pid = node.header()->next_leaf;
    }
    return results;
}

// ---------------------------------------------------------------
// Insert helpers
// ---------------------------------------------------------------
void BTree::insert_in_leaf(PageID leaf_pid, int64_t key, RID rid) {
    BTreeNode node = load_node(leaf_pid);
    uint16_t n = node.num_keys();
    if (n >= MAX_KEYS) return; // Safety check
    
    // Shift keys/rids right to make room
    uint16_t i = n;
    while (i > 0 && node.get_key(i-1) > key) {
        node.set_key(i, node.get_key(i-1));
        node.set_rid(i, node.get_rid(i-1));
        i--;
    }
    node.set_key(i, key);
    node.set_rid(i, rid);
    node.header()->num_keys = n + 1;
    save_node(leaf_pid, node);
}

void BTree::split_leaf(PageID leaf_pid, int64_t &out_key, PageID &out_right) {
    BTreeNode left = load_node(leaf_pid);
    uint16_t  n    = left.num_keys();
    uint16_t  mid  = n / 2;

    out_right = alloc_node(true);
    BTreeNode right = load_node(out_right);

    // Copy upper half to right node
    uint16_t j = 0;
    for (uint16_t i = mid; i < n; i++, j++) {
        right.set_key(j, left.get_key(i));
        right.set_rid(j, left.get_rid(i));
    }
    right.header()->num_keys  = j;
    right.header()->next_leaf = left.header()->next_leaf;
    left.header()->num_keys   = mid;
    left.header()->next_leaf  = out_right;

    out_key = right.get_key(0);

    save_node(leaf_pid,   left);
    save_node(out_right,  right);
}

void BTree::split_internal(PageID node_pid, int64_t &out_key, PageID &out_right) {
    BTreeNode left = load_node(node_pid);
    uint16_t  n    = left.num_keys();
    uint16_t  mid  = n / 2;

    out_key   = left.get_key(mid);
    out_right = alloc_node(false);
    BTreeNode right = load_node(out_right);

    uint16_t j = 0;
    for (uint16_t i = mid + 1; i < n; i++, j++) {
        right.set_key(j, left.get_key(i));
        right.set_child(j, left.get_child(i));
    }
    right.set_child(j, left.get_child(n));
    right.header()->num_keys = j;
    left.header()->num_keys  = mid;

    save_node(node_pid,  left);
    save_node(out_right, right);
}

void BTree::insert_in_parent(PageID left, int64_t key, PageID right) {
    if (left == root_) {
        // Create new root
        PageID new_root = alloc_node(false);
        BTreeNode root_node = load_node(new_root);
        root_node.set_key(0, key);
        root_node.set_child(0, left);
        root_node.set_child(1, right);
        root_node.header()->num_keys = 1;
        save_node(new_root, root_node);
        root_ = new_root;
        return;
    }
    
    // Find parent by walking from root with careful boundary detection
    PageID parent_pid = root_;
    
    while (true) {
        BTreeNode node = load_node(parent_pid);
        if (node.is_leaf()) break;
        
        uint16_t n = node.num_keys();
        uint16_t i = 0;
        while (i < n && key >= node.get_key(i)) i++;
        
        PageID child_pid = node.get_child(i);
        
        // Check if we found the correct child containing left or right
        if (child_pid == left || child_pid == right) break;
        
        parent_pid = child_pid;
    }
    
    BTreeNode parent = load_node(parent_pid);
    uint16_t n = parent.num_keys();
    
    // Check if parent has space before inserting
    if (n >= MAX_KEYS) {
        // Parent is full, split it first
        int64_t push_key; 
        PageID new_right_parent;
        split_internal(parent_pid, push_key, new_right_parent);
        insert_in_parent(parent_pid, push_key, new_right_parent);
        // Re-insert into appropriate parent after split
        insert_in_parent(left, key, right);
        return;
    }
    
    // Shift keys and children right to make room
    uint16_t i = n;
    while (i > 0 && parent.get_key(i-1) > key) {
        parent.set_key(i, parent.get_key(i-1));
        parent.set_child(i+1, parent.get_child(i));
        i--;
    }
    parent.set_key(i, key);
    parent.set_child(i+1, right);
    parent.header()->num_keys = n + 1;
    save_node(parent_pid, parent);
}

// ---------------------------------------------------------------
// Insert
// ---------------------------------------------------------------
void BTree::insert(int64_t key, RID rid) {
    PageID leaf_pid = find_leaf(key);
    BTreeNode leaf  = load_node(leaf_pid);

    if (!leaf.is_full()) {
        insert_in_leaf(leaf_pid, key, rid);
        return;
    }
    // Leaf is full — insert then split
    insert_in_leaf(leaf_pid, key, rid);
    int64_t new_key; 
    PageID new_right;
    split_leaf(leaf_pid, new_key, new_right);
    insert_in_parent(leaf_pid, new_key, new_right);
}

// ---------------------------------------------------------------
// Remove (mark by invalidating RID — simplified)
// ---------------------------------------------------------------
bool BTree::remove(int64_t key, RID rid) {
    PageID pid = find_leaf(key);
    BTreeNode node = load_node(pid);
    for (uint16_t i = 0; i < node.num_keys(); i++) {
        if (node.get_key(i) == key) {
            RID r = node.get_rid(i);
            if (r.page_id == rid.page_id && r.slot == rid.slot) {
                // Shift left
                for (uint16_t j = i; j < node.num_keys()-1; j++) {
                    node.set_key(j, node.get_key(j+1));
                    node.set_rid(j, node.get_rid(j+1));
                }
                node.header()->num_keys--;
                save_node(pid, node);
                return true;
            }
        }
    }
    return false;
}
