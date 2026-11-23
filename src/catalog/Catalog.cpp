#include "Catalog.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <filesystem>

bool Catalog::create_table(TableDef def) {
    if (tables_.count(def.name)) return false;
    std::string name = def.name;
    tables_.emplace(std::move(name), std::move(def));
    return true;
}

bool Catalog::drop_table(const std::string &name) {
    return tables_.erase(name) > 0;
}

TableDef* Catalog::get_table(const std::string &name) {
    auto it = tables_.find(name);
    return it == tables_.end() ? nullptr : &it->second;
}

const TableDef* Catalog::get_table(const std::string &name) const {
    auto it = tables_.find(name);
    return it == tables_.end() ? nullptr : &it->second;
}

bool Catalog::table_exists(const std::string &name) const {
    return tables_.count(name) > 0;
}

std::vector<std::string> Catalog::list_tables() const {
    std::vector<std::string> names;
    for (auto &[name, _] : tables_) names.push_back(name);
    return names;
}

// File format (one record per line, identifiers never contain spaces):
//   TABLE <name>
//   COLUMN <name> <TYPE> <nullable 0|1> <primary_key 0|1>
//   INDEX <column>
//   END
void Catalog::save(const std::string &path) const {
    std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out) throw std::runtime_error("Catalog: cannot write " + tmp);
        for (auto &[name, def] : tables_) {
            out << "TABLE " << name << "\n";
            for (auto &c : def.columns)
                out << "COLUMN " << c.name << " " << dtype_str(c.type) << " "
                    << (c.nullable ? 1 : 0) << " " << (c.is_primary_key ? 1 : 0) << "\n";
            for (auto &ic : def.indexed_columns)
                out << "INDEX " << ic << "\n";
            out << "END\n";
        }
    }
    // Write-then-rename so a crash mid-save never leaves a half-written catalog.
    // Some MinGW versions refuse to rename over an existing file, so fall back
    // to remove-then-rename there.
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        std::filesystem::remove(path, ec);
        std::filesystem::rename(tmp, path);
    }
}

void Catalog::load(const std::string &path) {
    tables_.clear();
    std::ifstream in(path);
    if (!in) return;
    std::string line;
    TableDef cur;
    bool in_table = false;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::string kw;
        ss >> kw;
        if (kw == "TABLE") {
            cur = TableDef();
            ss >> cur.name;
            in_table = true;
        } else if (kw == "COLUMN" && in_table) {
            std::string name, type;
            int nullable = 1, pk = 0;
            ss >> name >> type >> nullable >> pk;
            auto dt = dtype_from_str(type);
            if (!dt) throw std::runtime_error("Catalog: bad type '" + type + "' in " + path);
            Column c(name, *dt, nullable != 0);
            c.is_primary_key = pk != 0;
            cur.columns.push_back(c);
        } else if (kw == "INDEX" && in_table) {
            std::string col;
            ss >> col;
            cur.indexed_columns.push_back(col);
        } else if (kw == "END" && in_table) {
            tables_[cur.name] = cur;
            in_table = false;
        }
    }
}
