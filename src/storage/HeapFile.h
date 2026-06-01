#pragma once
#include <string>
#include <memory>
#include <functional>
#include "BufferPool.h"
#include "../catalog/Type.h"

// RID — uniquely identifies a row: which page + which slot
struct RID {
    PageID   page_id = INVALID_PAGE_ID;
    uint16_t slot    = 0;
    bool valid() const { return page_id != INVALID_PAGE_ID; }
};

class HeapFile {
public:
    HeapFile(const std::string &name,
             std::shared_ptr<BufferPool> pool,
             std::shared_ptr<DiskManager> disk);

    // Insert raw tuple bytes — returns RID of inserted row
    RID insert(const char *data, uint16_t len);

    // Fetch tuple by RID — copies into out_buf, returns length or 0
    uint16_t fetch(RID rid, char *out_buf, uint16_t buf_size) const;

    // Delete tuple by RID
    bool remove(RID rid);

    // Scan all live tuples — calls callback(rid, data, len) for each
    void scan(std::function<void(RID, const char*, uint16_t)> callback) const;

private:
    // Find or create a page with enough free space
    Frame* find_or_create_page(uint16_t needed, PageID &out_pid);

    std::string                  name_;
    std::shared_ptr<BufferPool>  pool_;
    std::shared_ptr<DiskManager> disk_;
    PageID                       first_page_ = INVALID_PAGE_ID;
};
