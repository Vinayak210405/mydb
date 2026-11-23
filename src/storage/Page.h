#pragma once
#include <cstdint>
#include <cstring>
#include "DiskManager.h"

// ---------------------------------------------------------------------------
// Slotted page (the layout Postgres and most row stores use)
//
//   +-------------+------------------------>        <------------------------+
//   | PageHeader  | slot 0 | slot 1 | ...    free     ... | tuple 1 | tuple 0 |
//   +-------------+------------------------>        <------------------------+
//                  slot directory grows →              ← tuple data grows
//
// A row is addressed by (page id, slot index) — its RID. Deleting a row sets
// its slot length to 0; the slot index can be reused by a later insert, and
// the space is reclaimed by compact() when the page runs out of room.
// ---------------------------------------------------------------------------
constexpr size_t SLOT_SIZE = 4;

struct PageHeader {
    uint32_t page_id     = 0;
    uint16_t num_slots   = 0;
    uint16_t free_offset = 0;   // start of tuple data; 0 means "fresh page"
    uint32_t flags       = 0;
    uint32_t reserved    = 0;
};
static_assert(sizeof(PageHeader) == 16, "PageHeader must be 16 bytes");

struct Slot {
    uint16_t offset = 0;
    uint16_t length = 0;   // 0 = empty / deleted
};

class Page {
public:
    Page() { std::memset(data_, 0, PAGE_SIZE); }

    char*       raw()       { return data_; }
    const char* raw() const { return data_; }

    PageHeader*       header()       { return reinterpret_cast<PageHeader*>(data_); }
    const PageHeader* header() const { return reinterpret_cast<const PageHeader*>(data_); }

    // Insert a tuple; returns its slot index or -1 if it does not fit.
    int         insert(const char *tuple_data, uint16_t len);
    // Pointer to a live tuple, or nullptr if the slot is empty/out of range.
    const char* get(uint16_t slot_idx, uint16_t &out_len) const;
    bool        remove(uint16_t slot_idx);
    // Replace a tuple in place (same slot). Returns false if it cannot fit.
    bool        update(uint16_t slot_idx, const char *tuple_data, uint16_t len);

    // Would a tuple of `len` bytes fit (possibly after compaction)?
    bool        can_fit(uint16_t len) const;
    uint16_t    free_space() const;     // total reclaimable free bytes
    uint16_t    live_count() const;     // number of live tuples

    static constexpr uint16_t MAX_TUPLE_SIZE =
        static_cast<uint16_t>(PAGE_SIZE - sizeof(PageHeader) - SLOT_SIZE);

private:
    Slot* slot(uint16_t idx) {
        return reinterpret_cast<Slot*>(data_ + sizeof(PageHeader) + idx * SLOT_SIZE);
    }
    const Slot* slot(uint16_t idx) const {
        return reinterpret_cast<const Slot*>(data_ + sizeof(PageHeader) + idx * SLOT_SIZE);
    }
    uint16_t free_offset() const {
        return header()->free_offset == 0 ? static_cast<uint16_t>(PAGE_SIZE)
                                          : header()->free_offset;
    }
    int  find_free_slot() const;
    void compact();
    bool place(uint16_t slot_idx, const char *tuple_data, uint16_t len);

    char data_[PAGE_SIZE];
};
