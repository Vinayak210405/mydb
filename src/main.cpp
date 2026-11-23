#include "server/Net.h"
#include "server/Server.h"
#include "server/Connection.h"
#include "server/Api.h"
#include "engine/Database.h"
#include "query/QueryEngine.h"
#include <iostream>
#include <csignal>
#include <thread>
#include <filesystem>

namespace fs = std::filesystem;

static TcpServer *g_sql_server  = nullptr;
static TcpServer *g_http_server = nullptr;

static void handle_signal(int) {
    std::cout << "\n[mydb] shutting down...\n";
    if (g_sql_server)  g_sql_server->stop();
    if (g_http_server) g_http_server->stop();
}

static void usage() {
    std::cout <<
        "usage: mydb [options]\n"
        "  --data DIR     data directory            (default: data)\n"
        "  --web DIR      web console files          (default: web)\n"
        "  --host ADDR    address to listen on       (default: 127.0.0.1)\n"
        "  --port N       binary protocol port       (default: 5433)\n"
        "  --http N       web console / HTTP API port (default: 8080)\n";
}

// Find the web/ folder whether mydb is started from the repo root or from build/.
static std::string find_web_dir(const std::string &requested, const char *argv0) {
    if (fs::exists(fs::path(requested) / "index.html")) return requested;
    fs::path exe_dir = fs::absolute(argv0).parent_path();
    for (fs::path candidate : {exe_dir / "web", exe_dir.parent_path() / "web",
                               exe_dir.parent_path().parent_path() / "web"})
        if (fs::exists(candidate / "index.html")) return candidate.string();
    return requested;
}

int main(int argc, char *argv[]) {
    std::string data_dir = "data", web_dir = "web", host = "127.0.0.1";
    uint16_t port = 5433, http_port = 8080;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) { usage(); std::exit(2); }
            return argv[++i];
        };
        try {
            if (a == "--data")      data_dir = next();
            else if (a == "--web")  web_dir = next();
            else if (a == "--host") host = next();
            else if (a == "--port") port = static_cast<uint16_t>(std::stoi(next()));
            else if (a == "--http") http_port = static_cast<uint16_t>(std::stoi(next()));
            else if (a == "--help" || a == "-h") { usage(); return 0; }
            else { std::cerr << "unknown option " << a << "\n"; usage(); return 2; }
        } catch (std::exception &) {
            std::cerr << "invalid value for " << a << "\n";
            return 2;
        }
    }
    web_dir = find_web_dir(web_dir, argv[0]);

    net::WinsockSession winsock;
    if (!winsock.ok()) {
        std::cerr << "[mydb] WSAStartup failed\n";
        return 1;
    }

    Database db(data_dir);
    QueryEngine engine(db);
    ApiHandler api(engine, web_dir);

    TcpServer sql_server("sql", host, port, [&engine](SOCKET s) { handle_binary_client(s, engine); });
    TcpServer http_server("http", host, http_port, [&api](SOCKET s) { handle_http_client(s, api); });
    if (!sql_server.init() || !http_server.init()) return 1;
    g_sql_server = &sql_server;
    g_http_server = &http_server;
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    std::string shown_host = host == "0.0.0.0" ? "localhost" : host;
    std::cout << "mydb is running\n"
              << "  data directory : " << fs::absolute(data_dir).string() << "\n"
              << "  tables loaded  : " << db.catalog().table_count() << "\n"
              << "  web console    : http://" << shown_host << ":" << http_port << "/\n"
              << "  SQL protocol   : " << host << ":" << port << "  (tools/client)\n"
              << "Press Ctrl+C to stop." << std::endl;
    if (!fs::exists(fs::path(web_dir) / "index.html"))
        std::cout << "warning: web console files not found in '" << web_dir
                  << "' — start mydb from the repo folder or pass --web <dir>\n";

    std::thread sql_thread([&]() { sql_server.run(); });
    http_server.run();
    sql_server.stop();
    sql_thread.join();
    engine.with_lock([](Database &d) { d.flush_all(); return 0; });
    std::cout << "[mydb] bye\n";
    return 0;
}
