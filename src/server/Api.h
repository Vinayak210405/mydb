#pragma once
#include <string>
#include "Http.h"
#include "../query/QueryEngine.h"

// ---------------------------------------------------------------------------
// The web console's HTTP API (and the static files of the console itself).
//
//   POST /api/query                    body = SQL (one or more statements)
//   GET  /api/schema                   tables, columns, indexes, sizes
//   GET  /api/index?table=t&column=c   B+ tree nodes, level by level
//   GET  /api/pages?table=t            slotted-page occupancy of a table file
//   GET  /api/stats                    buffer pool and disk I/O counters
//   GET  /<file>                       files from the web/ directory
//
// No sockets here — HttpServer feeds requests in, so this is unit-testable.
// ---------------------------------------------------------------------------
class ApiHandler {
public:
    ApiHandler(QueryEngine &engine, std::string web_dir)
        : engine_(engine), web_dir_(std::move(web_dir)) {}

    HttpResponse handle(const HttpRequest &req);

private:
    HttpResponse query(const HttpRequest &req);
    HttpResponse schema();
    HttpResponse index(const HttpRequest &req);
    HttpResponse pages(const HttpRequest &req);
    HttpResponse stats();
    HttpResponse static_file(const std::string &path);

    QueryEngine &engine_;
    std::string  web_dir_;
};

// Serialises query results the same way /api/query does (exposed for tests).
std::string results_to_json(const std::vector<QueryResult> &results);
