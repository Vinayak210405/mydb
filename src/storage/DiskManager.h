#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <cstdint>

using PageID = uint32_t;
constexpr PageID INVALID_PAGE_ID = UINT32_MAX;
constexpr size_t PAGE_SIZE = 4096;

// ---------------------------------------------------------------------------
// DiskManager — reads and writes fixed-size 4 KB pages of ONE file.
// Page N lives at byte offset N * PAGE_SIZE. Every table and every index has
// its own file, and therefore its own DiskManager.
// ---------------------------------------------------------------------------
class DiskManager {
public:
    explicit DiskManager(const std::string &filepath);
    ~DiskManager();

    DiskManager(const DiskManager&) = delete;
    DiskManager& operator=(const DiskManager&) = delete;

    void   write_page(PageID page_id, const char *data);
    void   read_page(PageID page_id, char *data);
    PageID allocate_page();
    PageID page_count() const;

    const std::string& path() const { return filepath_; }

    // I/O counters (shown in the web console's storage panel)
    uint64_t reads()  const { return reads_; }
    uint64_t writes() const { return writes_; }

private:
    std::fstream       file_;
    std::string        filepath_;
    PageID             page_count_ { 0 };
    uint64_t           reads_  { 0 };
    uint64_t           writes_ { 0 };
    mutable std::mutex mutex_;
};
