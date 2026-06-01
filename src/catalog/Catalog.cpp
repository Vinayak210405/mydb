#include "Catalog.h"

bool Catalog::create_table(TableDef def) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (tables_.count(def.name)) return false;
    std::string name = def.name;
    tables_.emplace(std::move(name), std::move(def));
    return true;
}

bool Catalog::drop_table(const std::string &name) {
    std::lock_guard<std::mutex> lock(mutex_);
    return tables_.erase(name) > 0;
}

const TableDef* Catalog::get_table(const std::string &name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tables_.find(name);
    if (it == tables_.end()) return nullptr;
    return &it->second;
}

bool Catalog::table_exists(const std::string &name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tables_.count(name) > 0;
}

std::vector<std::string> Catalog::list_tables() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    for (auto &[name, _] : tables_)
        names.push_back(name);
    return names;
}

size_t Catalog::table_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tables_.size();
}
