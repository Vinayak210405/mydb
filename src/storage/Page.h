#pragma once
#include <cstdint>
#include <cstring>
#include "DiskManager.h"

constexpr size_t SLOT_SIZE = 4;

struct PageHeader {
    uint32_t page_id     = 0;
    uint16_t num_slots   = 0;
    uint16_t free_offset = static_cast<uint16_t>(PAGE_SIZE);
    uint32_t flags       = 0;
    uint32_t reserved    = 0;
};

struct Slot {
    uint16_t offset = 0;
    uint16_t length = 0;
};

class Page {
public:
    Page() { std::memset(data_, 0, PAGE_SIZE); }

    char*       raw()       { return data_; }
    const char* raw() const { return data_; }

    PageHeader*       header()       { return reinterpret_cast<PageHeader*>(data_); }
    const PageHeader* header() const { return reinterpret_cast<const PageHeader*>(data_); }

    int          insert(const char *tuple_data, uint16_t len);
    const char*  get(uint16_t slot_idx, uint16_t &out_len) const;
    bool         remove(uint16_t slot_idx);
    uint16_t     free_space() const;

private:
    Slot* slot(uint16_t idx) {
        return reinterpret_cast<Slot*>(data_ + sizeof(PageHeader) + idx * SLOT_SIZE);
    }
    const Slot* slot(uint16_t idx) const {
        return reinterpret_cast<const Slot*>(data_ + sizeof(PageHeader) + idx * SLOT_SIZE);
    }

    char data_[PAGE_SIZE];
};
