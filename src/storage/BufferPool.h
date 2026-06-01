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

class BufferPool {
public:
    BufferPool(std::shared_ptr<DiskManager> disk, size_t pool_size);
    ~BufferPool();

    Frame* fetch_page(PageID page_id);
    Frame* new_page(PageID &out_page_id);
    void   unpin(PageID page_id, bool dirty);
    void   flush_page(PageID page_id);
    void   flush_all();

private:
    Frame* evict();

    std::shared_ptr<DiskManager>         disk_;
    size_t                               pool_size_;
    std::vector<Frame>                   frames_;
    std::unordered_map<PageID, size_t>   page_table_;
    std::list<size_t>                    lru_;
    mutable std::mutex                   mutex_;
};
