#include "index/BTreeNode.h"
#include "storage/HeapFile.h"
#include <cstring>

int64_t BTreeNode::get_key(uint16_t i) const {
    if (i >= MAX_KEYS) return -1;
    int64_t k;
    std::memcpy(&k, data_ + KEY_OFFSET + i * sizeof(int64_t), sizeof(int64_t));
    return k;
}

void BTreeNode::set_key(uint16_t i, int64_t key) {
    if (i >= MAX_KEYS) return;
    std::memcpy(data_ + KEY_OFFSET + i * sizeof(int64_t), &key, sizeof(int64_t));
}

RID BTreeNode::get_rid(uint16_t i) const {
    if (i >= MAX_KEYS) return RID{INVALID_PAGE_ID, 0};
    RID rid;
    std::memcpy(&rid, data_ + VALUE_OFFSET + i * sizeof(RID), sizeof(RID));
    return rid;
}

void BTreeNode::set_rid(uint16_t i, RID rid) {
    if (i >= MAX_KEYS) return;
    std::memcpy(data_ + VALUE_OFFSET + i * sizeof(RID), &rid, sizeof(RID));
}

PageID BTreeNode::get_child(uint16_t i) const {
    if (i > MAX_KEYS) return INVALID_PAGE_ID;
    PageID pid;
    std::memcpy(&pid, data_ + VALUE_OFFSET + i * sizeof(PageID), sizeof(PageID));
    return pid;
}

void BTreeNode::set_child(uint16_t i, PageID pid) {
    if (i > MAX_KEYS) return;
    std::memcpy(data_ + VALUE_OFFSET + i * sizeof(PageID), &pid, sizeof(PageID));
}
