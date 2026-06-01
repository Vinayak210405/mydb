#include "BufferPool.h"
#include <stdexcept>

BufferPool::BufferPool(std::shared_ptr<DiskManager> disk, size_t pool_size)
    : disk_(std::move(disk)), pool_size_(pool_size), frames_(pool_size) {}

BufferPool::~BufferPool() { flush_all(); }

Frame* BufferPool::fetch_page(PageID page_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Already in pool?
    auto it = page_table_.find(page_id);
    if (it != page_table_.end()) {
        size_t idx = it->second;
        frames_[idx].pin_count++;
        lru_.remove(idx);
        lru_.push_back(idx);
        return &frames_[idx];
    }

    // Need to load from disk — find a free frame
    Frame *frame = evict();
    if (!frame) throw std::runtime_error("BufferPool: all frames pinned");

    disk_->read_page(page_id, frame->page.raw());
    frame->page_id   = page_id;
    frame->pin_count = 1;
    frame->dirty     = false;

    size_t idx = frame - frames_.data();
    page_table_[page_id] = idx;
    lru_.push_back(idx);
    return frame;
}

Frame* BufferPool::new_page(PageID &out_page_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    Frame *frame = evict();
    if (!frame) throw std::runtime_error("BufferPool: all frames pinned");

    PageID pid = disk_->allocate_page();
    frame->page_id   = pid;
    frame->pin_count = 1;
    frame->dirty     = true;
    frame->page      = Page();
    frame->page.header()->page_id = pid;

    size_t idx = frame - frames_.data();
    page_table_[pid] = idx;
    lru_.push_back(idx);

    out_page_id = pid;
    return frame;
}

void BufferPool::unpin(PageID page_id, bool dirty) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) return;
    Frame &f = frames_[it->second];
    if (f.pin_count > 0) f.pin_count--;
    if (dirty) f.dirty = true;
}

void BufferPool::flush_page(PageID page_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) return;
    Frame &f = frames_[it->second];
    if (f.dirty) {
        disk_->write_page(page_id, f.page.raw());
        f.dirty = false;
    }
}

void BufferPool::flush_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto &[pid, idx] : page_table_) {
        Frame &f = frames_[idx];
        if (f.dirty) {
            disk_->write_page(pid, f.page.raw());
            f.dirty = false;
        }
    }
}

Frame* BufferPool::evict() {
    // Find unpinned frame in LRU order
    for (auto it = lru_.begin(); it != lru_.end(); ++it) {
        Frame &f = frames_[*it];
        if (f.pin_count == 0) {
            if (f.dirty && f.page_id != INVALID_PAGE_ID)
                disk_->write_page(f.page_id, f.page.raw());
            if (f.page_id != INVALID_PAGE_ID)
                page_table_.erase(f.page_id);
            lru_.erase(it);
            f = Frame{};
            return &f;
        }
    }
    // No evictable frame — use a never-used frame
    for (auto &f : frames_)
        if (f.page_id == INVALID_PAGE_ID) return &f;
    return nullptr;
}
