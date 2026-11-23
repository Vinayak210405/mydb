#include "HeapFile.h"
#include <stdexcept>
#include <cstring>

HeapFile::HeapFile(const std::string &path, size_t pool_frames)
    : disk_(std::make_shared<DiskManager>(path)),
      pool_(std::make_unique<BufferPool>(disk_, pool_frames)) {
    // Build the free-space map once at open time.
    PageID n = disk_->page_count();
    free_map_.resize(n);
    for (PageID pid = 0; pid < n; pid++) {
        PageGuard g(*pool_, pid);
        free_map_[pid] = g.page().free_space();
    }
}

void HeapFile::set_free(PageID pid, uint16_t free_bytes) {
    if (pid >= free_map_.size()) free_map_.resize(pid + 1, 0);
    free_map_[pid] = free_bytes;
}

RID HeapFile::insert(const char *data, uint16_t len) {
    if (len > Page::MAX_TUPLE_SIZE)
        throw std::runtime_error("row too large (" + std::to_string(len) +
                                 " bytes; max " + std::to_string(Page::MAX_TUPLE_SIZE) + ")");
    // First page whose free-space map entry says it has room.
    for (PageID pid = 0; pid < free_map_.size(); pid++) {
        if (free_map_[pid] < len + SLOT_SIZE) continue;
        PageGuard g(*pool_, pid);
        int slot = g.page().insert(data, len);
        if (slot >= 0) {
            g.mark_dirty();
            set_free(pid, g.page().free_space());
            return RID{pid, static_cast<uint16_t>(slot)};
        }
        set_free(pid, g.page().free_space());   // map was stale; correct it
    }
    // No room anywhere: append a new page.
    PageID pid;
    Frame *f = pool_->new_page(pid);
    int slot = f->page.insert(data, len);
    set_free(pid, f->page.free_space());
    pool_->unpin(pid, true);
    if (slot < 0) throw std::runtime_error("HeapFile: failed to insert row");
    return RID{pid, static_cast<uint16_t>(slot)};
}

bool HeapFile::fetch(RID rid, std::vector<char> &out) {
    if (!rid.valid() || rid.page_id >= disk_->page_count()) return false;
    PageGuard g(*pool_, rid.page_id);
    uint16_t len = 0;
    const char *p = g.page().get(rid.slot, len);
    if (!p) return false;
    out.assign(p, p + len);
    return true;
}

bool HeapFile::remove(RID rid) {
    if (!rid.valid() || rid.page_id >= disk_->page_count()) return false;
    PageGuard g(*pool_, rid.page_id);
    bool ok = g.page().remove(rid.slot);
    if (ok) {
        g.mark_dirty();
        set_free(rid.page_id, g.page().free_space());
    }
    return ok;
}

RID HeapFile::update(RID rid, const char *data, uint16_t len) {
    {
        PageGuard g(*pool_, rid.page_id);
        if (g.page().update(rid.slot, data, len)) {
            g.mark_dirty();
            set_free(rid.page_id, g.page().free_space());
            return rid;
        }
    }
    // Did not fit on its page: move it.
    if (!remove(rid)) throw std::runtime_error("HeapFile: update of missing row");
    return insert(data, len);
}

void HeapFile::scan(const std::function<void(RID, const char*, uint16_t)> &cb) {
    PageID total = disk_->page_count();
    for (PageID pid = 0; pid < total; pid++) {
        PageGuard g(*pool_, pid);
        uint16_t num_slots = g.page().header()->num_slots;
        for (uint16_t s = 0; s < num_slots; s++) {
            uint16_t len = 0;
            const char *data = g.page().get(s, len);
            if (data) cb(RID{pid, s}, data, len);
        }
    }
}

bool HeapFile::Cursor::next(RID &rid, std::vector<char> &row) {
    PageID total = hf_->disk_->page_count();
    while (pid_ < total) {
        PageGuard g(*hf_->pool_, pid_);
        uint16_t num_slots = g.page().header()->num_slots;
        while (slot_ < num_slots) {
            uint16_t s = slot_++;
            uint16_t len = 0;
            const char *data = g.page().get(s, len);
            if (data) {
                rid = RID{pid_, s};
                row.assign(data, data + len);
                return true;
            }
        }
        pid_++;
        slot_ = 0;
    }
    return false;
}

size_t HeapFile::row_count() {
    size_t n = 0;
    PageID total = disk_->page_count();
    for (PageID pid = 0; pid < total; pid++) {
        PageGuard g(*pool_, pid);
        n += g.page().live_count();
    }
    return n;
}
