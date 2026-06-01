#include "Page.h"

int Page::insert(const char *tuple_data, uint16_t len) {
    // Check if there is enough space
    uint16_t slots_end = static_cast<uint16_t>(
        sizeof(PageHeader) + (header()->num_slots + 1) * SLOT_SIZE);
    uint16_t new_free_offset = static_cast<uint16_t>(
        header()->free_offset - len);

    if (new_free_offset < slots_end) return -1;  // no space

    // Write tuple at new_free_offset
    header()->free_offset = new_free_offset;
    std::memcpy(data_ + new_free_offset, tuple_data, len);

    // Write slot
    uint16_t slot_idx = header()->num_slots++;
    slot(slot_idx)->offset = new_free_offset;
    slot(slot_idx)->length = len;

    return static_cast<int>(slot_idx);
}

const char* Page::get(uint16_t slot_idx, uint16_t &out_len) const {
    if (slot_idx >= header()->num_slots) return nullptr;
    const Slot *s = slot(slot_idx);
    if (s->length == 0) return nullptr;  // deleted
    out_len = s->length;
    return data_ + s->offset;
}

bool Page::remove(uint16_t slot_idx) {
    if (slot_idx >= header()->num_slots) return false;
    slot(slot_idx)->length = 0;  // mark deleted
    return true;
}

uint16_t Page::free_space() const {
    uint16_t slots_end = static_cast<uint16_t>(
        sizeof(PageHeader) + header()->num_slots * SLOT_SIZE);
    return header()->free_offset - slots_end;
}
