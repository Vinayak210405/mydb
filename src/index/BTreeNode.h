#pragma once
#include <vector>
#include <cstdint>
#include <cstring>
#include "../storage/DiskManager.h"
#include "../catalog/Type.h"
#include "../storage/HeapFile.h"

constexpr uint16_t BTREE_ORDER = 64;  // max keys per node

struct RID;

// ------------------------------------------------------------------
// BTreeNode — stored as one page on disk
//
// Layout inside the 4KB page:
//   [is_leaf: 1 byte]
//   [num_keys: 2 bytes]
//   [parent_page: 4 bytes]
//   [keys: BTREE_ORDER * 8 bytes]       (int64 keys)
//   [rids/children: BTREE_ORDER * 6 bytes]
//     leaf:     RID (4+2 bytes) per key
//     internal: PageID (4 bytes) per child (num_keys+1 children)
// ------------------------------------------------------------------

struct BNodeHeader {
    uint8_t  is_leaf    = 1;
    uint16_t num_keys   = 0;
    PageID   parent     = INVALID_PAGE_ID;
    PageID   next_leaf  = INVALID_PAGE_ID;  // leaf linked list
};

class BTreeNode {
public:
    BTreeNode() { std::memset(data_, 0, PAGE_SIZE); }

    char*       raw()       { return data_; }
    const char* raw() const { return data_; }

    BNodeHeader* header() {
        return reinterpret_cast<BNodeHeader*>(data_);
    }
    const BNodeHeader* header() const {
        return reinterpret_cast<const BNodeHeader*>(data_);
    }

    bool     is_leaf()   const { return header()->is_leaf != 0; }
    uint16_t num_keys()  const { return header()->num_keys; }
    bool     is_full()   const { return num_keys() >= BTREE_ORDER - 1; }

    // Key access
    int64_t  get_key(uint16_t i) const;
    void     set_key(uint16_t i, int64_t key);

    // Leaf: RID access
    RID      get_rid(uint16_t i) const;
    void     set_rid(uint16_t i, RID rid);

    // Internal: child PageID access
    PageID   get_child(uint16_t i) const;
    void     set_child(uint16_t i, PageID pid);

private:
    // Offsets inside data_
    static constexpr size_t KEY_OFFSET   = sizeof(BNodeHeader);
    static constexpr size_t VALUE_OFFSET = KEY_OFFSET + BTREE_ORDER * sizeof(int64_t);

    char data_[PAGE_SIZE];
};
