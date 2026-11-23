#include "DiskManager.h"
#include <filesystem>
#include <stdexcept>
#include <cstring>

DiskManager::DiskManager(const std::string &filepath) : filepath_(filepath) {
    auto parent = std::filesystem::path(filepath).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent);

    file_.open(filepath, std::ios::in | std::ios::out | std::ios::binary);
    if (!file_.is_open()) {
        // File does not exist yet: create it.
        file_.open(filepath, std::ios::in | std::ios::out |
                             std::ios::binary | std::ios::trunc);
        if (!file_.is_open())
            throw std::runtime_error("DiskManager: cannot open " + filepath);
        page_count_ = 0;
    } else {
        file_.seekg(0, std::ios::end);
        auto size = file_.tellg();
        page_count_ = static_cast<PageID>(size / PAGE_SIZE);
    }
}

DiskManager::~DiskManager() {
    if (file_.is_open()) {
        file_.flush();
        file_.close();
    }
}

void DiskManager::write_page(PageID page_id, const char *data) {
    std::lock_guard<std::mutex> lock(mutex_);
    file_.seekp(static_cast<std::streamoff>(page_id) * PAGE_SIZE);
    file_.write(data, PAGE_SIZE);
    file_.flush();
    if (!file_) throw std::runtime_error("DiskManager: write failed on " + filepath_);
    writes_++;
}

void DiskManager::read_page(PageID page_id, char *data) {
    std::lock_guard<std::mutex> lock(mutex_);
    file_.clear();
    file_.seekg(static_cast<std::streamoff>(page_id) * PAGE_SIZE);
    file_.read(data, PAGE_SIZE);
    auto got = file_.gcount();
    if (got < static_cast<std::streamsize>(PAGE_SIZE))
        std::memset(data + got, 0, PAGE_SIZE - static_cast<size_t>(got));
    file_.clear();   // reading past EOF sets failbit; reset it
    reads_++;
}

PageID DiskManager::allocate_page() {
    std::lock_guard<std::mutex> lock(mutex_);
    PageID new_id = page_count_++;
    // Extend the file with a zeroed page so page_count() is correct on reopen.
    char buf[PAGE_SIZE];
    std::memset(buf, 0, PAGE_SIZE);
    file_.seekp(static_cast<std::streamoff>(new_id) * PAGE_SIZE);
    file_.write(buf, PAGE_SIZE);
    if (!file_) throw std::runtime_error("DiskManager: allocate failed on " + filepath_);
    return new_id;
}

PageID DiskManager::page_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return page_count_;
}
