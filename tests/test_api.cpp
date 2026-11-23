#include "TestFramework.h"
#include "server/Api.h"
#include "server/Http.h"
#include "server/Json.h"
#include <filesystem>
#include <fstream>

static HttpRequest request(const std::string &method, const std::string &target, const std::string &body = "") {
    auto req = http::parse_head(method + " " + target + " HTTP/1.1\r\nHost: x\r\n\r\n");
    req->body = body;
    return *req;
}

TEST(parse_request_head) {
    auto r = http::parse_head("GET /api/index?table=my%20t&column=id HTTP/1.1\r\nContent-Length: 12\r\nX-A:  b\r\n\r\n");
    CHECK(r.has_value());
    CHECK_EQ(r->method, std::string("GET"));
    CHECK_EQ(r->path, std::string("/api/index"));
    CHECK_EQ(r->param("table"), std::string("my t"));
    CHECK_EQ(r->param("column"), std::string("id"));
    CHECK_EQ(r->headers["content-length"], std::string("12"));
    CHECK_EQ(r->headers["x-a"], std::string("b"));
    CHECK(!http::parse_head("garbage\r\n\r\n").has_value());
    CHECK(!http::parse_head("GET noslash HTTP/1.1\r\n\r\n").has_value());
}

TEST(json_escaping) {
    JsonWriter w;
    w.begin_object().key("s").value(std::string("a\"b\\c\n\x01")).key("n").null()
     .key("f").value(1.5).key("arr").begin_array().value(int64_t(1)).value(true).end_array().end_object();
    CHECK_EQ(w.str(), std::string("{\"s\":\"a\\\"b\\\\c\\n\\u0001\",\"n\":null,\"f\":1.5,\"arr\":[1,true]}"));
}

TEST(query_endpoint) {
    Database db(mini_test::temp_dir("api_query"));
    QueryEngine e(db);
    ApiHandler api(e, "web");
    auto r = api.handle(request("POST", "/api/query",
        "CREATE TABLE t (id INT PRIMARY KEY, v VARCHAR); INSERT INTO t VALUES (1, 'x'); SELECT * FROM t WHERE id = 1;"));
    CHECK_EQ(r.status, 200);
    CHECK(r.body.find("\"rows\":[[1,\"x\"]]") != std::string::npos);
    CHECK(r.body.find("\"name\":\"IndexScan\"") != std::string::npos);

    auto bad = api.handle(request("POST", "/api/query", "SELECT * FROM missing"));
    CHECK(bad.body.find("\"ok\":false") != std::string::npos);
    CHECK_EQ(api.handle(request("GET", "/api/query")).status, 405);
    CHECK_EQ(api.handle(request("POST", "/api/query")).status, 400);
}

TEST(introspection_endpoints) {
    Database db(mini_test::temp_dir("api_introspect"));
    QueryEngine e(db);
    ApiHandler api(e, "web");
    e.execute("CREATE TABLE t (id INT PRIMARY KEY, v VARCHAR)");
    e.execute("INSERT INTO t VALUES (1, 'a'), (2, 'b')");
    auto schema = api.handle(request("GET", "/api/schema"));
    CHECK(schema.body.find("\"name\":\"t\",\"rows\":2") != std::string::npos);
    auto idx = api.handle(request("GET", "/api/index?table=t&column=id"));
    CHECK(idx.body.find("\"keys\":[1,2]") != std::string::npos);
    CHECK_EQ(api.handle(request("GET", "/api/index?table=t&column=v")).status, 500);
    CHECK(api.handle(request("GET", "/api/pages?table=t")).body.find("\"live\":2") != std::string::npos);
    CHECK(api.handle(request("GET", "/api/stats")).body.find("t.id.idx") != std::string::npos);
    CHECK_EQ(api.handle(request("GET", "/api/unknown")).status, 404);
}

TEST(static_files_stay_inside_web_dir) {
    std::string dir = mini_test::temp_dir("api_static");
    std::ofstream(dir + "/index.html") << "<h1>hi</h1>";
    std::ofstream(dir + "/../mydb_secret.txt") << "secret";
    Database db(mini_test::temp_dir("api_static_db"));
    QueryEngine e(db);
    ApiHandler api(e, dir);
    auto home = api.handle(request("GET", "/"));
    CHECK_EQ(home.status, 200);
    CHECK_EQ(home.body, std::string("<h1>hi</h1>"));
    CHECK(home.content_type.find("text/html") != std::string::npos);
    CHECK_EQ(api.handle(request("GET", "/../mydb_secret.txt")).status, 404);
    CHECK_EQ(api.handle(request("GET", "/%2e%2e/mydb_secret.txt")).status, 404);
    CHECK_EQ(api.handle(request("GET", "/nope.js")).status, 404);
}

TEST_MAIN()
