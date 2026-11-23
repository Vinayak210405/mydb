#include "Api.h"
#include "Json.h"
#include <fstream>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace {

HttpResponse json_error(const std::string &msg, int status) {
    JsonWriter w;
    w.begin_object().key("error").value(msg).end_object();
    return HttpResponse::json(w.str(), status);
}

void write_plan(JsonWriter &w, const PlanNode &n) {
    w.begin_object()
        .key("name").value(n.name)
        .key("detail").value(n.detail)
        .key("rows").value(n.rows)
        .key("children").begin_array();
    for (auto &c : n.children) write_plan(w, c);
    w.end_array().end_object();
}

uint64_t file_size(const std::string &path) {
    std::error_code ec;
    auto s = fs::file_size(path, ec);
    return ec ? 0 : static_cast<uint64_t>(s);
}

}  // namespace

std::string results_to_json(const std::vector<QueryResult> &results) {
    JsonWriter w;
    w.begin_object().key("results").begin_array();
    for (auto &r : results) {
        w.begin_object()
            .key("ok").value(r.ok)
            .key("sql").value(r.sql)
            .key("kind").value(r.kind)
            .key("message").value(r.message)
            .key("elapsed_ms").value(r.elapsed_ms)
            .key("rows_affected").value(r.rows_affected)
            .key("columns").begin_array();
        for (auto &c : r.columns) w.value(c);
        w.end_array().key("types").begin_array();
        for (auto &t : r.column_types) w.value(t);
        w.end_array().key("rows").begin_array();
        for (auto &row : r.rows) {
            w.begin_array();
            for (auto &v : row) w.value(v);
            w.end_array();
        }
        w.end_array().key("plan");
        if (r.plan) write_plan(w, *r.plan);
        else w.null();
        w.end_object();
    }
    w.end_array().end_object();
    return w.str();
}

HttpResponse ApiHandler::handle(const HttpRequest &req) {
    try {
        const std::string &p = req.path;
        if (p == "/api/query") {
            if (req.method != "POST") return json_error("use POST", 405);
            return query(req);
        }
        if (p.rfind("/api/", 0) == 0 && req.method != "GET") return json_error("use GET", 405);
        if (p == "/api/schema") return schema();
        if (p == "/api/index")  return index(req);
        if (p == "/api/pages")  return pages(req);
        if (p == "/api/stats")  return stats();
        if (p.rfind("/api/", 0) == 0) return json_error("unknown endpoint " + p, 404);
        if (req.method != "GET" && req.method != "HEAD") return HttpResponse::text("method not allowed", 405);
        return static_file(p == "/" ? "/index.html" : p);
    } catch (std::exception &e) {
        return json_error(e.what(), 500);
    }
}

HttpResponse ApiHandler::query(const HttpRequest &req) {
    if (req.body.empty()) return json_error("empty query", 400);
    return HttpResponse::json(results_to_json(engine_.execute_script(req.body)));
}

HttpResponse ApiHandler::schema() {
    std::string body = engine_.with_lock([](Database &db) {
        JsonWriter w;
        w.begin_object().key("tables").begin_array();
        for (auto &name : db.catalog().list_tables()) {
            const TableDef *def = db.catalog().get_table(name);
            HeapFile &heap = db.heap(name);
            w.begin_object()
                .key("name").value(name)
                .key("rows").value(static_cast<uint64_t>(heap.row_count()))
                .key("pages").value(static_cast<uint64_t>(heap.page_count()))
                .key("bytes").value(file_size(db.table_path(name)))
                .key("columns").begin_array();
            for (auto &c : def->columns) {
                w.begin_object()
                    .key("name").value(c.name)
                    .key("type").value(dtype_str(c.type))
                    .key("nullable").value(c.nullable)
                    .key("primary_key").value(c.is_primary_key)
                    .key("indexed").value(def->has_index(c.name))
                    .end_object();
            }
            w.end_array().key("indexes").begin_array();
            for (auto &col : def->indexed_columns) {
                BTree *bt = db.index(name, col);
                w.begin_object()
                    .key("column").value(col)
                    .key("height").value(bt->height())
                    .key("nodes").value(static_cast<uint64_t>(bt->node_count()))
                    .key("entries").value(static_cast<uint64_t>(bt->entry_count()))
                    .key("bytes").value(file_size(db.index_path(name, col)))
                    .end_object();
            }
            w.end_array().end_object();
        }
        w.end_array().end_object();
        return w.str();
    });
    return HttpResponse::json(body);
}

HttpResponse ApiHandler::index(const HttpRequest &req) {
    std::string table = req.param("table"), column = req.param("column");
    std::string body = engine_.with_lock([&](Database &db) -> std::string {
        BTree *bt = db.index(table, column);
        if (!bt) throw std::runtime_error("no index on " + table + "." + column);
        auto levels = bt->levels(400);
        JsonWriter w;
        w.begin_object()
            .key("table").value(table)
            .key("column").value(column)
            .key("height").value(bt->height())
            .key("entries").value(static_cast<uint64_t>(bt->entry_count()))
            .key("max_keys").value(static_cast<int64_t>(MAX_KEYS - 1))
            .key("root").value(static_cast<uint64_t>(bt->root()))
            .key("levels").begin_array();
        for (auto &level : levels) {
            w.begin_array();
            for (auto &n : level) {
                w.begin_object()
                    .key("page").value(static_cast<uint64_t>(n.page))
                    .key("leaf").value(n.leaf)
                    .key("keys").begin_array();
                for (auto k : n.keys) w.value(k);
                w.end_array().key("children").begin_array();
                for (auto c : n.children) w.value(static_cast<uint64_t>(c));
                w.end_array().key("next");
                if (n.next_leaf == INVALID_PAGE_ID) w.null();
                else w.value(static_cast<uint64_t>(n.next_leaf));
                w.end_object();
            }
            w.end_array();
        }
        w.end_array().end_object();
        return w.str();
    });
    return HttpResponse::json(body);
}

HttpResponse ApiHandler::pages(const HttpRequest &req) {
    std::string table = req.param("table");
    std::string body = engine_.with_lock([&](Database &db) -> std::string {
        HeapFile &heap = db.heap(table);
        PageID n = heap.page_count();
        JsonWriter w;
        w.begin_object()
            .key("table").value(table)
            .key("page_size").value(static_cast<uint64_t>(PAGE_SIZE))
            .key("page_count").value(static_cast<uint64_t>(n))
            .key("pages").begin_array();
        for (PageID pid = 0; pid < n && pid < 512; pid++) {
            PageGuard g(heap.pool(), pid);
            Page &p = g.page();
            w.begin_object()
                .key("page").value(static_cast<uint64_t>(pid))
                .key("slots").value(static_cast<int64_t>(p.header()->num_slots))
                .key("live").value(static_cast<int64_t>(p.live_count()))
                .key("free").value(static_cast<int64_t>(p.free_space()))
                .end_object();
        }
        w.end_array().end_object();
        return w.str();
    });
    return HttpResponse::json(body);
}

HttpResponse ApiHandler::stats() {
    std::string body = engine_.with_lock([](Database &db) {
        JsonWriter w;
        w.begin_object().key("files").begin_array();
        auto file = [&](const std::string &name, const std::string &kind, BufferPool &pool) {
            DiskManager &d = pool.disk();
            w.begin_object()
                .key("name").value(name)
                .key("kind").value(kind)
                .key("pages").value(static_cast<uint64_t>(d.page_count()))
                .key("disk_reads").value(d.reads())
                .key("disk_writes").value(d.writes())
                .key("hits").value(pool.hits())
                .key("misses").value(pool.misses())
                .key("cached").value(static_cast<uint64_t>(pool.pages_cached()))
                .key("pool_size").value(static_cast<uint64_t>(pool.pool_size()))
                .end_object();
        };
        for (auto &name : db.catalog().list_tables()) {
            file(name + ".tbl", "table", db.heap(name).pool());
            for (auto &col : db.catalog().get_table(name)->indexed_columns)
                file(name + "." + col + ".idx", "index", db.index(name, col)->pool());
        }
        w.end_array().key("data_dir").value(db.data_dir()).end_object();
        return w.str();
    });
    return HttpResponse::json(body);
}

HttpResponse ApiHandler::static_file(const std::string &path) {
    // Only plain relative file names under web/ — no "..", no absolute paths.
    if (path.find("..") != std::string::npos || path.find('\\') != std::string::npos ||
        path.find(':') != std::string::npos)
        return HttpResponse::text("not found", 404);
    fs::path full = fs::path(web_dir_) / path.substr(1);
    std::ifstream in(full, std::ios::binary);
    if (!in) return HttpResponse::text("not found: " + path, 404);
    std::ostringstream ss;
    ss << in.rdbuf();
    HttpResponse r;
    r.content_type = http::content_type_for(path);
    r.body = ss.str();
    return r;
}
