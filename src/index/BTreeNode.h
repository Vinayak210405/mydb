#pragma once
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include "../storage/DiskManager.h"
#include "../storage/RID.h"

// ---------------------------------------------------------------------------
// B+ tree node — a view over one 4 KB page (no copy: it points straight into
// the buffer pool frame).
//
// Layout inside the page:
//   [BNodeHeader: 8 bytes]
//   [keys     : MAX_KEYS * int64]
//   [values   : leaf     -> MAX_KEYS RIDs, one per key
//               internal -> MAX_KEYS + 1 child PageIDs]
//
// A node may briefly hold MAX_KEYS keys while an insert is in progress; it is
// split immediately, so at rest every node holds at most MAX_KEYS - 1 keys.
// ---------------------------------------------------------------------------
constexpr uint16_t MAX_KEYS = 64;

struct BNodeHeader {
    uint8_t  is_leaf   = 1;
    uint8_t  reserved  = 0;
    uint16_t num_keys  = 0;
    PageID   next_leaf = INVALID_PAGE_ID;   // leaves form a linked list for range scans
};
static_assert(sizeof(BNodeHeader) == 8, "BNodeHeader must be 8 bytes");

class BTreeNode {
public:
    explicit BTreeNode(char *page_data) : d_(page_data) {}

    static constexpr size_t KEY_OFFSET   = sizeof(BNodeHeader);
    static constexpr size_t VALUE_OFFSET = KEY_OFFSET + MAX_KEYS * sizeof(int64_t);

    void init(bool leaf) {
        std::memset(d_, 0, PAGE_SIZE);
        BNodeHeader h;
        h.is_leaf = leaf ? 1 : 0;
        std::memcpy(d_, &h, sizeof(h));
    }

    BNodeHeader*       header()       { return reinterpret_cast<BNodeHeader*>(d_); }
    const BNodeHeader* header() const { return reinterpret_cast<const BNodeHeader*>(d_); }

    bool     is_leaf()  const { return header()->is_leaf != 0; }
    uint16_t num_keys() const { return header()->num_keys; }
    void     set_num_keys(uint16_t n) { header()->num_keys = n; }

    int64_t get_key(uint16_t i) const {
        check(i < MAX_KEYS, "key");
        int64_t k; std::memcpy(&k, d_ + KEY_OFFSET + i * sizeof(int64_t), sizeof k); return k;
    }
    void set_key(uint16_t i, int64_t k) {
        check(i < MAX_KEYS, "key");
        std::memcpy(d_ + KEY_OFFSET + i * sizeof(int64_t), &k, sizeof k);
    }

    RID get_rid(uint16_t i) const {
        check(i < MAX_KEYS, "rid");
        RID r; std::memcpy(&r, d_ + VALUE_OFFSET + i * sizeof(RID), sizeof r); return r;
    }
    void set_rid(uint16_t i, RID r) {
        check(i < MAX_KEYS, "rid");
        std::memcpy(d_ + VALUE_OFFSET + i * sizeof(RID), &r, sizeof r);
    }

    PageID get_child(uint16_t i) const {
        check(i <= MAX_KEYS, "child");
        PageID p; std::memcpy(&p, d_ + VALUE_OFFSET + i * sizeof(PageID), sizeof p); return p;
    }
    void set_child(uint16_t i, PageID p) {
        check(i <= MAX_KEYS, "child");
        std::memcpy(d_ + VALUE_OFFSET + i * sizeof(PageID), &p, sizeof p);
    }

private:
    static void check(bool ok, const char *what) {
        if (!ok) throw std::out_of_range(std::string("BTreeNode: ") + what + " index out of range");
    }
    char *d_;
};

static_assert(BTreeNode::VALUE_OFFSET + MAX_KEYS * sizeof(RID) <= PAGE_SIZE, "leaf overflows page");
static_assert(BTreeNode::VALUE_OFFSET + (MAX_KEYS + 1) * sizeof(PageID) <= PAGE_SIZE, "internal overflows page");
