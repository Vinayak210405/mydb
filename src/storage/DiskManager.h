#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <cstdint>

using PageID = uint32_t;
constexpr PageID INVALID_PAGE_ID = UINT32_MAX;
constexpr size_t PAGE_SIZE = 4096;

class DiskManager {
public:
    explicit DiskManager(const std::string &filepath);
    ~DiskManager();

    void write_page(PageID page_id, const char *data);
    void read_page(PageID page_id, char *data);
    PageID allocate_page();
    PageID page_count() const { return page_count_; }

private:
    std::fstream       file_;
    std::string        filepath_;
    PageID             page_count_ { 0 };
    mutable std::mutex mutex_;
};
