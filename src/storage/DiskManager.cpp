#include "DiskManager.h"
#include <filesystem>
#include <stdexcept>
#include <cstring>

DiskManager::DiskManager(const std::string &filepath) : filepath_(filepath) {
    std::filesystem::create_directories(
        std::filesystem::path(filepath).parent_path());
    file_.open(filepath, std::ios::in | std::ios::out | std::ios::binary);
    if (!file_.is_open()) {
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
    if (file_.is_open()) file_.close();
}

void DiskManager::write_page(PageID page_id, const char *data) {
    std::lock_guard<std::mutex> lock(mutex_);
    file_.seekp(static_cast<std::streamoff>(page_id) * PAGE_SIZE);
    file_.write(data, PAGE_SIZE);
    file_.flush();
}

void DiskManager::read_page(PageID page_id, char *data) {
    std::lock_guard<std::mutex> lock(mutex_);
    file_.seekg(static_cast<std::streamoff>(page_id) * PAGE_SIZE);
    file_.read(data, PAGE_SIZE);
    if (file_.gcount() < (std::streamsize)PAGE_SIZE)
        std::memset(data + file_.gcount(), 0, PAGE_SIZE - file_.gcount());
}

PageID DiskManager::allocate_page() {
    std::lock_guard<std::mutex> lock(mutex_);
    PageID new_id = page_count_++;
    file_.seekp(static_cast<std::streamoff>(new_id) * PAGE_SIZE);
    char buf[PAGE_SIZE];
    std::memset(buf, 0, PAGE_SIZE);
    file_.write(buf, PAGE_SIZE);
    file_.flush();
    return new_id;
}
