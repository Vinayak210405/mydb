#include "index/BTreeNode.h"
#include "storage/HeapFile.h"
#include <cstring>
#include <stdexcept>

int64_t BTreeNode::get_key(uint16_t i) const {
    if (i >= MAX_KEYS) {
        throw std::out_of_range("BTreeNode::get_key index out of bounds");
    }
    int64_t k;
    std::memcpy(&k, data_ + KEY_OFFSET + i * sizeof(int64_t), sizeof(int64_t));
    return k;
}

void BTreeNode::set_key(uint16_t i, int64_t key) {
    if (i >= MAX_KEYS) {
        throw std::out_of_range("BTreeNode::set_key index out of bounds");
    }
    std::memcpy(data_ + KEY_OFFSET + i * sizeof(int64_t), &key, sizeof(int64_t));
}

RID BTreeNode::get_rid(uint16_t i) const {
    if (i >= MAX_KEYS) {
        throw std::out_of_range("BTreeNode::get_rid index out of bounds");
    }
    RID rid;
    std::memcpy(&rid, data_ + VALUE_OFFSET + i * sizeof(RID), sizeof(RID));
    return rid;
}

void BTreeNode::set_rid(uint16_t i, RID rid) {
    if (i >= MAX_KEYS) {
        throw std::out_of_range("BTreeNode::set_rid index out of bounds");
    }
    std::memcpy(data_ + VALUE_OFFSET + i * sizeof(RID), &rid, sizeof(RID));
}

PageID BTreeNode::get_child(uint16_t i) const {
    if (i > MAX_KEYS) {  // Children can go up to MAX_KEYS (one more than keys)
        throw std::out_of_range("BTreeNode::get_child index out of bounds");
    }
    PageID pid;
    std::memcpy(&pid, data_ + VALUE_OFFSET + i * sizeof(PageID), sizeof(PageID));
    return pid;
}

void BTreeNode::set_child(uint16_t i, PageID pid) {
    if (i > MAX_KEYS) {  // Children can go up to MAX_KEYS (one more than keys)
        throw std::out_of_range("BTreeNode::set_child index out of bounds");
    }
    std::memcpy(data_ + VALUE_OFFSET + i * sizeof(PageID), &pid, sizeof(PageID));
}
