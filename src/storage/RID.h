#pragma once
#include <cstdint>
#include "DiskManager.h"

// RID (record id) — where a row lives: which page of the table file, which slot.
struct RID {
    PageID   page_id = INVALID_PAGE_ID;
    uint16_t slot    = 0;

    bool valid() const { return page_id != INVALID_PAGE_ID; }
    bool operator==(const RID &o) const { return page_id == o.page_id && slot == o.slot; }
    bool operator!=(const RID &o) const { return !(*this == o); }
};
