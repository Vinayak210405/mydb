#include "BufferPool.h"
#include <stdexcept>

BufferPool::BufferPool(std::shared_ptr<DiskManager> disk, size_t pool_size)
    : disk_(std::move(disk)), pool_size_(pool_size), frames_(pool_size),
      lru_pos_(pool_size), in_lru_(pool_size, false) {
    for (size_t i = pool_size; i-- > 0;) free_list_.push_back(i);
}

BufferPool::~BufferPool() {
    try { flush_all(); } catch (...) {}
}

void BufferPool::touch(size_t idx) {
    if (in_lru_[idx]) lru_.erase(lru_pos_[idx]);
    lru_.push_back(idx);
    lru_pos_[idx] = std::prev(lru_.end());
    in_lru_[idx]  = true;
}

Frame* BufferPool::acquire_frame() {
    if (!free_list_.empty()) {
        size_t idx = free_list_.back();
        free_list_.pop_back();
        return &frames_[idx];
    }
    // Evict the least recently used frame that nobody has pinned.
    for (auto it = lru_.begin(); it != lru_.end(); ++it) {
        Frame &f = frames_[*it];
        if (f.pin_count != 0) continue;
        if (f.dirty) disk_->write_page(f.page_id, f.page.raw());
        page_table_.erase(f.page_id);
        in_lru_[*it] = false;
        lru_.erase(it);
        f = Frame{};
        return &f;
    }
    return nullptr;
}

Frame* BufferPool::fetch_page(PageID page_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = page_table_.find(page_id);
    if (it != page_table_.end()) {
        hits_++;
        Frame &f = frames_[it->second];
        f.pin_count++;
        touch(it->second);
        return &f;
    }
    misses_++;
    Frame *frame = acquire_frame();
    if (!frame) throw std::runtime_error("BufferPool: all frames pinned");

    disk_->read_page(page_id, frame->page.raw());
    frame->page_id   = page_id;
    frame->pin_count = 1;
    frame->dirty     = false;
    size_t idx = static_cast<size_t>(frame - frames_.data());
    page_table_[page_id] = idx;
    touch(idx);
    return frame;
}

Frame* BufferPool::new_page(PageID &out_page_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    Frame *frame = acquire_frame();
    if (!frame) throw std::runtime_error("BufferPool: all frames pinned");

    PageID pid = disk_->allocate_page();
    frame->page      = Page();
    frame->page.header()->page_id = pid;
    frame->page_id   = pid;
    frame->pin_count = 1;
    frame->dirty     = true;
    size_t idx = static_cast<size_t>(frame - frames_.data());
    page_table_[pid] = idx;
    touch(idx);
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

size_t BufferPool::pages_cached() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return page_table_.size();
}
