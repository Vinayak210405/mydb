#include "Page.h"
#include <vector>
#include <algorithm>

int Page::find_free_slot() const {
    for (uint16_t i = 0; i < header()->num_slots; i++)
        if (slot(i)->length == 0) return i;
    return -1;
}

uint16_t Page::free_space() const {
    size_t used = sizeof(PageHeader) + header()->num_slots * SLOT_SIZE;
    for (uint16_t i = 0; i < header()->num_slots; i++) used += slot(i)->length;
    return static_cast<uint16_t>(PAGE_SIZE - used);
}

uint16_t Page::live_count() const {
    uint16_t n = 0;
    for (uint16_t i = 0; i < header()->num_slots; i++)
        if (slot(i)->length > 0) n++;
    return n;
}

bool Page::can_fit(uint16_t len) const {
    size_t need = len + (find_free_slot() < 0 ? SLOT_SIZE : 0);
    return free_space() >= need;
}

// Slide all live tuples to the end of the page so the free space is one
// contiguous gap. Slot indexes (and therefore RIDs) do not change.
void Page::compact() {
    struct Live { uint16_t idx; std::vector<char> bytes; };
    std::vector<Live> live;
    for (uint16_t i = 0; i < header()->num_slots; i++) {
        Slot *s = slot(i);
        if (s->length > 0)
            live.push_back({i, std::vector<char>(data_ + s->offset, data_ + s->offset + s->length)});
    }
    uint16_t off = static_cast<uint16_t>(PAGE_SIZE);
    for (auto &l : live) {
        off = static_cast<uint16_t>(off - l.bytes.size());
        std::memcpy(data_ + off, l.bytes.data(), l.bytes.size());
        slot(l.idx)->offset = off;
    }
    header()->free_offset = off;
}

bool Page::place(uint16_t slot_idx, const char *tuple_data, uint16_t len) {
    uint16_t num_slots = header()->num_slots;
    bool new_slot      = slot_idx >= num_slots;
    size_t slots_end   = sizeof(PageHeader) + (new_slot ? num_slots + 1 : num_slots) * SLOT_SIZE;

    if (free_offset() < slots_end || free_offset() - slots_end < len) {
        compact();
        if (free_offset() < slots_end || free_offset() - slots_end < len) return false;
    }
    uint16_t off = static_cast<uint16_t>(free_offset() - len);
    std::memcpy(data_ + off, tuple_data, len);
    header()->free_offset = off;
    if (new_slot) header()->num_slots = static_cast<uint16_t>(num_slots + 1);
    slot(slot_idx)->offset = off;
    slot(slot_idx)->length = len;
    return true;
}

int Page::insert(const char *tuple_data, uint16_t len) {
    if (len == 0 || !can_fit(len)) return -1;
    int idx = find_free_slot();
    uint16_t slot_idx = idx >= 0 ? static_cast<uint16_t>(idx) : header()->num_slots;
    return place(slot_idx, tuple_data, len) ? slot_idx : -1;
}

const char* Page::get(uint16_t slot_idx, uint16_t &out_len) const {
    if (slot_idx >= header()->num_slots) return nullptr;
    const Slot *s = slot(slot_idx);
    if (s->length == 0) return nullptr;
    out_len = s->length;
    return data_ + s->offset;
}

bool Page::remove(uint16_t slot_idx) {
    if (slot_idx >= header()->num_slots) return false;
    if (slot(slot_idx)->length == 0) return false;
    slot(slot_idx)->length = 0;
    slot(slot_idx)->offset = 0;
    return true;
}

bool Page::update(uint16_t slot_idx, const char *tuple_data, uint16_t len) {
    if (slot_idx >= header()->num_slots || slot(slot_idx)->length == 0 || len == 0) return false;
    Slot *s = slot(slot_idx);
    if (len <= s->length) {                       // shrink or same size: overwrite in place
        std::memcpy(data_ + s->offset, tuple_data, len);
        s->length = len;
        return true;
    }
    // Grows: does it fit once the old copy is freed?
    if (free_space() + s->length < len) return false;
    std::vector<char> old(data_ + s->offset, data_ + s->offset + s->length);
    s->length = 0;
    s->offset = 0;
    if (place(slot_idx, tuple_data, len)) return true;
    place(slot_idx, old.data(), static_cast<uint16_t>(old.size()));   // restore
    return false;
}
