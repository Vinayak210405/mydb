#pragma once

#include "../common.h"
#include "../storage/DiskManager.h"
#include <cstdint>
#include <cstring>

// Forward declare RID to avoid circular includes
struct RID;

// BTree Node Layout:
// Header (16 bytes): is_leaf, num_keys, next_leaf, reserved
// Keys (up to 128 * 8 bytes)
// Values (up to 128 * 8 bytes for RIDs or child pointers)

constexpr uint16_t MAX_KEYS = 128;
constexpr size_t KEY_OFFSET = 16;
constexpr size_t VALUE_OFFSET = KEY_OFFSET + MAX_KEYS * sizeof(int64_t);

struct BTreeNodeHeader {
    uint8_t  is_leaf;
    uint16_t num_keys;
    uint32_t next_leaf;
    uint8_t  reserved[7];
};

class BTreeNode {
public:
    BTreeNode() { std::memset(data_, 0, PAGE_SIZE); }

    BTreeNodeHeader* header() { return reinterpret_cast<BTreeNodeHeader*>(data_); }
    const BTreeNodeHeader* header() const { return reinterpret_cast<const BTreeNodeHeader*>(data_); }

    bool is_leaf() const { return header()->is_leaf != 0; }
    bool is_full() const { return header()->num_keys >= MAX_KEYS; }
    uint16_t num_keys() const { return header()->num_keys; }

    // Key accessors with bounds checking
    int64_t get_key(uint16_t i) const;
    void set_key(uint16_t i, int64_t key);

    // RID accessors (for leaf nodes)
    RID get_rid(uint16_t i) const;
    void set_rid(uint16_t i, RID rid);

    // Child pointer accessors (for internal nodes)
    PageID get_child(uint16_t i) const;
    void set_child(uint16_t i, PageID pid);

    // Raw data access
    uint8_t* raw() { return data_; }
    const uint8_t* raw() const { return data_; }

private:
    uint8_t data_[PAGE_SIZE];
};
