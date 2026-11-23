#pragma once
// A tiny test framework: TEST(name) { CHECK(...); CHECK_EQ(a, b); }
// Each test binary returns non-zero if any check failed, so CTest reports it.
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <functional>
#include <filesystem>

namespace mini_test {

struct Case { std::string name; std::function<void()> fn; };
inline std::vector<Case>& registry() { static std::vector<Case> r; return r; }
inline int& failures() { static int f = 0; return f; }

struct Register {
    Register(const char *name, std::function<void()> fn) { registry().push_back({name, std::move(fn)}); }
};

inline void fail(const char *file, int line, const std::string &msg) {
    std::cerr << "    FAIL " << file << ":" << line << ": " << msg << "\n";
    failures()++;
}

// A fresh, empty directory for one test's data files.
inline std::string temp_dir(const std::string &name) {
    auto dir = std::filesystem::temp_directory_path() / ("mydb_test_" + name);
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir.string();
}

inline int run_all() {
    int failed_tests = 0;
    for (auto &c : registry()) {
        int before = failures();
        try {
            c.fn();
        } catch (std::exception &e) {
            fail(c.name.c_str(), 0, std::string("uncaught exception: ") + e.what());
        }
        bool ok = failures() == before;
        if (!ok) failed_tests++;
        std::cout << (ok ? "  ok   " : "  FAIL ") << c.name << "\n";
    }
    std::cout << registry().size() - failed_tests << "/" << registry().size() << " tests passed\n";
    return failed_tests == 0 ? 0 : 1;
}

}  // namespace mini_test

#define MT_CONCAT2(a, b) a##b
#define MT_CONCAT(a, b) MT_CONCAT2(a, b)
#define TEST(name)                                                                  \
    static void name();                                                             \
    static mini_test::Register MT_CONCAT(reg_, name)(#name, name);                  \
    static void name()

#define CHECK(cond)                                                                 \
    do { if (!(cond)) mini_test::fail(__FILE__, __LINE__, "CHECK(" #cond ")"); } while (0)

#define CHECK_EQ(a, b)                                                              \
    do {                                                                            \
        auto _va = (a); auto _vb = (b);                                             \
        if (!(_va == _vb)) {                                                        \
            std::ostringstream _ss;                                                 \
            _ss << #a " == " #b " (got " << _va << " vs " << _vb << ")";            \
            mini_test::fail(__FILE__, __LINE__, _ss.str());                         \
        }                                                                           \
    } while (0)

#define CHECK_THROWS(expr)                                                          \
    do {                                                                            \
        bool _threw = false;                                                        \
        try { expr; } catch (...) { _threw = true; }                                \
        if (!_threw) mini_test::fail(__FILE__, __LINE__, "expected exception: " #expr); \
    } while (0)

#define TEST_MAIN() int main() { return mini_test::run_all(); }
