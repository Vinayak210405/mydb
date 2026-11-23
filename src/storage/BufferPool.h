#pragma once
#include <unordered_map>
#include <list>
#include <vector>
#include <memory>
#include <mutex>
#include "Page.h"
#include "DiskManager.h"

struct Frame {
    Page     page;
    PageID   page_id   = INVALID_PAGE_ID;
    int      pin_count = 0;
    bool     dirty     = false;
};

// ---------------------------------------------------------------------------
// BufferPool — a fixed number of in-memory frames caching pages of one file.
//
//  * fetch_page() pins a page (loading it from disk on a miss).
//  * unpin() releases it; pass dirty=true if you modified it.
//  * When every frame is in use, the least-recently-used UNPINNED frame is
//    evicted, writing it back to disk first if it is dirty.
// ---------------------------------------------------------------------------
class BufferPool {
public:
    BufferPool(std::shared_ptr<DiskManager> disk, size_t pool_size);
    ~BufferPool();

    BufferPool(const BufferPool&) = delete;
    BufferPool& operator=(const BufferPool&) = delete;

    Frame* fetch_page(PageID page_id);
    Frame* new_page(PageID &out_page_id);
    void   unpin(PageID page_id, bool dirty);
    void   flush_page(PageID page_id);
    void   flush_all();

    DiskManager& disk() { return *disk_; }
    size_t   pool_size() const { return pool_size_; }
    uint64_t hits()      const { return hits_; }
    uint64_t misses()    const { return misses_; }
    size_t   pages_cached() const;

private:
    Frame* acquire_frame();      // a free frame, or evict an unpinned one
    void   touch(size_t idx);    // move to most-recently-used end

    std::shared_ptr<DiskManager>              disk_;
    size_t                                    pool_size_;
    std::vector<Frame>                        frames_;
    std::unordered_map<PageID, size_t>        page_table_;   // page id → frame idx
    std::list<size_t>                         lru_;          // front = least recently used
    std::vector<std::list<size_t>::iterator>  lru_pos_;
    std::vector<bool>                         in_lru_;
    std::vector<size_t>                       free_list_;
    uint64_t                                  hits_   = 0;
    uint64_t                                  misses_ = 0;
    mutable std::mutex                        mutex_;
};

// RAII pin: unpins automatically when it goes out of scope.
class PageGuard {
public:
    PageGuard(BufferPool &pool, PageID pid) : pool_(&pool), pid_(pid), frame_(pool.fetch_page(pid)) {}
    ~PageGuard() { if (frame_) pool_->unpin(pid_, dirty_); }
    PageGuard(const PageGuard&) = delete;
    PageGuard& operator=(const PageGuard&) = delete;

    Page& page()       { return frame_->page; }
    void  mark_dirty() { dirty_ = true; }

private:
    BufferPool *pool_;
    PageID      pid_;
    Frame      *frame_;
    bool        dirty_ = false;
};
