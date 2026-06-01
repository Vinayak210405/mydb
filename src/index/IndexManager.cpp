#include "IndexManager.h"

IndexManager::IndexManager(std::shared_ptr<BufferPool>  pool,
                            std::shared_ptr<DiskManager> disk)
    : pool_(std::move(pool)), disk_(std::move(disk)) {}

void IndexManager::create_index(const std::string &table,
                                 const std::string &column) {
    std::string key = table + "." + column;
    if (indexes_.count(key)) return;
    indexes_[key] = std::make_shared<BTree>(pool_, disk_);
}

BTree* IndexManager::get_index(const std::string &table,
                                const std::string &column) {
    std::string key = table + "." + column;
    auto it = indexes_.find(key);
    if (it == indexes_.end()) return nullptr;
    return it->second.get();
}

bool IndexManager::has_index(const std::string &table,
                               const std::string &column) const {
    return indexes_.count(table + "." + column) > 0;
}
