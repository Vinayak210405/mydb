#pragma once
#include <string>
#include <memory>
#include <vector>
#include <functional>
#include "BufferPool.h"
#include "RID.h"

// ---------------------------------------------------------------------------
// HeapFile — an unordered collection of rows for ONE table, stored in its own
// file (data/<table>.tbl) as a sequence of slotted pages.
//
// It keeps a small in-memory free-space map (free bytes per page) so an
// insert can find a page with room without reading every page from disk.
// ---------------------------------------------------------------------------
class HeapFile {
public:
    HeapFile(const std::string &path, size_t pool_frames = 64);

    RID      insert(const char *data, uint16_t len);
    // Copies the row into `out`; returns false if the RID points at no row.
    bool     fetch(RID rid, std::vector<char> &out);
    bool     remove(RID rid);
    // Replace a row. The row keeps its RID if the new version fits on the same
    // page; otherwise it moves and the new RID is returned.
    RID      update(RID rid, const char *data, uint16_t len);

    void     scan(const std::function<void(RID, const char*, uint16_t)> &cb);

    // Iterator used by the SeqScan operator (Volcano-style, one row per call).
    class Cursor {
    public:
        explicit Cursor(HeapFile &hf) : hf_(&hf) {}
        bool next(RID &rid, std::vector<char> &row);
    private:
        HeapFile *hf_;
        PageID    pid_  = 0;
        uint16_t  slot_ = 0;
    };

    PageID      page_count() { return disk_->page_count(); }
    size_t      row_count();
    BufferPool& pool() { return *pool_; }
    DiskManager& disk() { return *disk_; }
    void        flush() { pool_->flush_all(); }

private:
    void set_free(PageID pid, uint16_t free_bytes);

    std::shared_ptr<DiskManager> disk_;
    std::unique_ptr<BufferPool>  pool_;
    std::vector<uint16_t>        free_map_;   // free bytes per page
};
