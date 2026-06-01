#include "HeapFile.h"
#include <stdexcept>
#include <cstring>

HeapFile::HeapFile(const std::string &name,
                   std::shared_ptr<BufferPool> pool,
                   std::shared_ptr<DiskManager> disk)
    : name_(name), pool_(std::move(pool)), disk_(std::move(disk)) {}

Frame* HeapFile::find_or_create_page(uint16_t needed, PageID &out_pid) {
    // Walk existing pages to find one with enough space
    PageID total = disk_->page_count();
    for (PageID pid = 0; pid < total; pid++) {
        Frame *f = pool_->fetch_page(pid);
        if (f->page.free_space() >= needed + SLOT_SIZE) {
            out_pid = pid;
            return f;
        }
        pool_->unpin(pid, false);
    }
    // No space found — allocate a new page
    Frame *f = pool_->new_page(out_pid);
    return f;
}

RID HeapFile::insert(const char *data, uint16_t len) {
    PageID pid;
    Frame *f = find_or_create_page(len, pid);
    int slot_idx = f->page.insert(data, len);
    pool_->unpin(pid, true);
    if (slot_idx < 0)
        throw std::runtime_error("HeapFile: failed to insert tuple");
    return RID{ pid, static_cast<uint16_t>(slot_idx) };
}

uint16_t HeapFile::fetch(RID rid, char *out_buf, uint16_t buf_size) const {
    Frame *f = pool_->fetch_page(rid.page_id);
    uint16_t len = 0;
    const char *data = f->page.get(rid.slot, len);
    if (data && len <= buf_size)
        std::memcpy(out_buf, data, len);
    else
        len = 0;
    pool_->unpin(rid.page_id, false);
    return len;
}

bool HeapFile::remove(RID rid) {
    Frame *f = pool_->fetch_page(rid.page_id);
    bool ok = f->page.remove(rid.slot);
    pool_->unpin(rid.page_id, ok);
    return ok;
}

void HeapFile::scan(std::function<void(RID, const char*, uint16_t)> cb) const {
    PageID total = disk_->page_count();
    for (PageID pid = 0; pid < total; pid++) {
        Frame *f = pool_->fetch_page(pid);
        uint16_t num_slots = f->page.header()->num_slots;
        for (uint16_t s = 0; s < num_slots; s++) {
            uint16_t len = 0;
            const char *data = f->page.get(s, len);
            if (data && len > 0)
                cb(RID{pid, s}, data, len);
        }
        pool_->unpin(pid, false);
    }
}
