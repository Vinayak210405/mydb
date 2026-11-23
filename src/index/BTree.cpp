#include "BTree.h"
#include <stdexcept>
#include <cstring>
#include <deque>

namespace {
constexpr char META_MAGIC[8] = {'M','Y','D','B','I','D','X','1'};
struct MetaPage {
    char   magic[8];
    PageID root;
};
}  // namespace

// ---------------------------------------------------------------------------
// Opening / meta page
// ---------------------------------------------------------------------------
BTree::BTree(const std::string &path, size_t pool_frames)
    : disk_(std::make_shared<DiskManager>(path)),
      pool_(std::make_unique<BufferPool>(disk_, pool_frames)) {
    if (disk_->page_count() == 0) {
        PageID meta_pid;
        Frame *f = pool_->new_page(meta_pid);           // page 0
        pool_->unpin(meta_pid, true);
        (void)f;
        set_root(alloc_node(/*leaf=*/true));
    } else {
        PageGuard g(*pool_, 0);
        MetaPage m;
        std::memcpy(&m, g.page().raw(), sizeof m);
        if (std::memcmp(m.magic, META_MAGIC, sizeof META_MAGIC) != 0)
            throw std::runtime_error("BTree: " + path + " is not a mydb index file");
        root_ = m.root;
    }
}

void BTree::set_root(PageID pid) {
    root_ = pid;
    PageGuard g(*pool_, 0);
    MetaPage m;
    std::memcpy(m.magic, META_MAGIC, sizeof META_MAGIC);
    m.root = pid;
    std::memcpy(g.page().raw(), &m, sizeof m);
    g.mark_dirty();
}

PageID BTree::alloc_node(bool leaf) {
    PageID pid;
    Frame *f = pool_->new_page(pid);
    BTreeNode(f->page.raw()).init(leaf);
    pool_->unpin(pid, true);
    return pid;
}

// ---------------------------------------------------------------------------
// Search
//
// Internal node with keys k0..k(n-1) and children c0..cn: child ci holds keys
// between k(i-1) and ki. Because duplicates of a separator can sit on BOTH
// sides of it after a split, searches descend to the LEFTMOST child that could
// contain the key (count of keys strictly less than it) and then walk right
// along the leaf chain.
// ---------------------------------------------------------------------------
PageID BTree::find_leaf_for_search(int64_t key) {
    PageID cur = root_;
    while (true) {
        PageGuard g(*pool_, cur);
        BTreeNode node(g.page().raw());
        if (node.is_leaf()) return cur;
        uint16_t i = 0;
        while (i < node.num_keys() && node.get_key(i) < key) i++;
        cur = node.get_child(i);
    }
}

std::vector<RID> BTree::lookup(int64_t key) {
    return range(key, key);
}

std::vector<RID> BTree::range(int64_t low, int64_t high) {
    std::vector<RID> out;
    if (low > high) return out;
    PageID pid = find_leaf_for_search(low);
    while (pid != INVALID_PAGE_ID) {
        PageGuard g(*pool_, pid);
        BTreeNode node(g.page().raw());
        for (uint16_t i = 0; i < node.num_keys(); i++) {
            int64_t k = node.get_key(i);
            if (k < low) continue;
            if (k > high) return out;
            out.push_back(node.get_rid(i));
        }
        pid = node.header()->next_leaf;
    }
    return out;
}

// ---------------------------------------------------------------------------
// Insert
// ---------------------------------------------------------------------------
void BTree::insert(int64_t key, RID rid) {
    // 1. Walk down to the leaf, remembering the internal nodes we pass.
    std::vector<PageID> path;
    PageID leaf_pid = root_;
    while (true) {
        PageGuard g(*pool_, leaf_pid);
        BTreeNode node(g.page().raw());
        if (node.is_leaf()) break;
        uint16_t i = 0;
        while (i < node.num_keys() && node.get_key(i) <= key) i++;
        path.push_back(leaf_pid);
        leaf_pid = node.get_child(i);
    }

    int64_t sep;
    PageID  left = leaf_pid, right;
    {
        // 2. Insert into the leaf in sorted position (after equal keys).
        PageGuard g(*pool_, leaf_pid);
        BTreeNode leaf(g.page().raw());
        uint16_t n = leaf.num_keys();
        uint16_t pos = n;
        while (pos > 0 && leaf.get_key(pos - 1) > key) {
            leaf.set_key(pos, leaf.get_key(pos - 1));
            leaf.set_rid(pos, leaf.get_rid(pos - 1));
            pos--;
        }
        leaf.set_key(pos, key);
        leaf.set_rid(pos, rid);
        leaf.set_num_keys(++n);
        g.mark_dirty();
        if (n < MAX_KEYS) return;

        // 3. Leaf is over-full: split it. The upper half moves to a new leaf,
        //    and the new leaf's first key is COPIED up as the separator.
        right = alloc_node(true);
        PageGuard rg(*pool_, right);
        BTreeNode rnode(rg.page().raw());
        uint16_t mid = n / 2, j = 0;
        for (uint16_t i = mid; i < n; i++, j++) {
            rnode.set_key(j, leaf.get_key(i));
            rnode.set_rid(j, leaf.get_rid(i));
        }
        rnode.set_num_keys(j);
        leaf.set_num_keys(mid);
        rnode.header()->next_leaf = leaf.header()->next_leaf;
        leaf.header()->next_leaf  = right;
        sep = rnode.get_key(0);
        rg.mark_dirty();
    }

    // 4. Insert (sep, right) into the parent, splitting upward as needed.
    while (true) {
        if (path.empty()) {
            // We split the root: grow the tree by one level.
            PageID new_root = alloc_node(false);
            PageGuard g(*pool_, new_root);
            BTreeNode r(g.page().raw());
            r.set_key(0, sep);
            r.set_child(0, left);
            r.set_child(1, right);
            r.set_num_keys(1);
            g.mark_dirty();
            set_root(new_root);
            return;
        }
        PageID parent_pid = path.back();
        path.pop_back();
        PageGuard g(*pool_, parent_pid);
        BTreeNode p(g.page().raw());
        uint16_t n = p.num_keys();

        uint16_t j = 0;                          // position of `left` among children
        while (j <= n && p.get_child(j) != left) j++;
        if (j > n) throw std::runtime_error("BTree: corrupt tree (child not found in parent)");

        for (uint16_t i = n; i > j; i--) {
            p.set_key(i, p.get_key(i - 1));
            p.set_child(i + 1, p.get_child(i));
        }
        p.set_key(j, sep);
        p.set_child(j + 1, right);
        p.set_num_keys(++n);
        g.mark_dirty();
        if (n < MAX_KEYS) return;

        // Internal split: the middle key is PUSHED up (it leaves both halves).
        PageID new_right = alloc_node(false);
        PageGuard rg(*pool_, new_right);
        BTreeNode r(rg.page().raw());
        uint16_t mid = n / 2, k = 0;
        int64_t up = p.get_key(mid);
        for (uint16_t i = mid + 1; i < n; i++, k++) {
            r.set_key(k, p.get_key(i));
            r.set_child(k, p.get_child(i));
        }
        r.set_child(k, p.get_child(n));
        r.set_num_keys(k);
        p.set_num_keys(mid);
        rg.mark_dirty();

        left  = parent_pid;
        right = new_right;
        sep   = up;
    }
}

// ---------------------------------------------------------------------------
// Remove one (key, rid) entry
// ---------------------------------------------------------------------------
bool BTree::remove(int64_t key, RID rid) {
    PageID pid = find_leaf_for_search(key);
    while (pid != INVALID_PAGE_ID) {
        PageGuard g(*pool_, pid);
        BTreeNode node(g.page().raw());
        uint16_t n = node.num_keys();
        for (uint16_t i = 0; i < n; i++) {
            int64_t k = node.get_key(i);
            if (k < key) continue;
            if (k > key) return false;
            if (node.get_rid(i) == rid) {
                for (uint16_t j = i; j + 1 < n; j++) {
                    node.set_key(j, node.get_key(j + 1));
                    node.set_rid(j, node.get_rid(j + 1));
                }
                node.set_num_keys(static_cast<uint16_t>(n - 1));
                g.mark_dirty();
                return true;
            }
        }
        pid = node.header()->next_leaf;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Introspection
// ---------------------------------------------------------------------------
std::vector<std::vector<BTree::NodeInfo>> BTree::levels(size_t max_nodes) {
    std::vector<std::vector<NodeInfo>> out;
    std::vector<PageID> frontier{root_};
    size_t seen = 0;
    while (!frontier.empty() && seen < max_nodes) {
        std::vector<NodeInfo> level;
        std::vector<PageID> next;
        for (PageID pid : frontier) {
            if (seen++ >= max_nodes) break;
            PageGuard g(*pool_, pid);
            BTreeNode node(g.page().raw());
            NodeInfo info{pid, node.is_leaf(), {}, {}, node.header()->next_leaf};
            for (uint16_t i = 0; i < node.num_keys(); i++) info.keys.push_back(node.get_key(i));
            if (!node.is_leaf()) {
                for (uint16_t i = 0; i <= node.num_keys(); i++) {
                    info.children.push_back(node.get_child(i));
                    next.push_back(node.get_child(i));
                }
            }
            level.push_back(std::move(info));
        }
        out.push_back(std::move(level));
        frontier = std::move(next);
    }
    return out;
}

int BTree::height() {
    int h = 1;
    PageID cur = root_;
    while (true) {
        PageGuard g(*pool_, cur);
        BTreeNode node(g.page().raw());
        if (node.is_leaf()) return h;
        cur = node.get_child(0);
        h++;
    }
}

size_t BTree::entry_count() {
    PageID cur = root_;
    while (true) {                               // leftmost leaf
        PageGuard g(*pool_, cur);
        BTreeNode node(g.page().raw());
        if (node.is_leaf()) break;
        cur = node.get_child(0);
    }
    size_t n = 0;
    while (cur != INVALID_PAGE_ID) {
        PageGuard g(*pool_, cur);
        BTreeNode node(g.page().raw());
        n += node.num_keys();
        cur = node.header()->next_leaf;
    }
    return n;
}
