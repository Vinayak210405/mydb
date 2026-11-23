#include "TestFramework.h"
#include "query/QueryEngine.h"
#include <filesystem>
#include <functional>

// Run SQL; fail the test if it errors.
static QueryResult ok(QueryEngine &e, const std::string &sql) {
    QueryResult r = e.execute(sql);
    if (!r.ok) mini_test::fail(__FILE__, __LINE__, "query failed: " + sql + " -> " + r.message);
    return r;
}
static std::string err(QueryEngine &e, const std::string &sql) {
    QueryResult r = e.execute(sql);
    if (r.ok) mini_test::fail(__FILE__, __LINE__, "expected an error: " + sql);
    return r.message;
}
static int64_t count(QueryEngine &e, const std::string &sql) {
    QueryResult r = ok(e, sql);
    return r.rows.empty() ? -1 : std::get<int64_t>(r.rows[0][0]);
}
static bool plan_has(const QueryResult &r, const std::string &op) {
    if (!r.plan) return false;
    std::function<bool(const PlanNode &)> walk = [&](const PlanNode &n) {
        if (n.name == op) return true;
        for (auto &c : n.children) if (walk(c)) return true;
        return false;
    };
    return walk(*r.plan);
}

static void seed(QueryEngine &e) {
    ok(e, "CREATE TABLE users (id INT PRIMARY KEY, name VARCHAR NOT NULL, age INT, city VARCHAR)");
    ok(e, "CREATE TABLE orders (oid INT PRIMARY KEY, user_id INT, amount FLOAT)");
    ok(e, "INSERT INTO users VALUES (1,'Alice',30,'Pune'),(2,'Bob',25,'Delhi'),(3,'Charlie',35,'Pune'),(4,'Dana',NULL,'Mumbai')");
    ok(e, "INSERT INTO orders VALUES (100,1,250.0),(101,1,99.5),(102,2,10),(103,3,500),(104,9,1)");
}

TEST(tables_do_not_leak_into_each_other) {
    Database db(mini_test::temp_dir("q_isolation"));
    QueryEngine e(db);
    seed(e);
    CHECK_EQ(count(e, "SELECT COUNT(*) FROM users"), 4);
    CHECK_EQ(count(e, "SELECT COUNT(*) FROM orders"), 5);
}

TEST(where_and_or_not_null) {
    Database db(mini_test::temp_dir("q_where"));
    QueryEngine e(db);
    seed(e);
    CHECK_EQ(count(e, "SELECT COUNT(*) FROM users WHERE id > 0 AND id < 3"), 2);
    CHECK_EQ(count(e, "SELECT COUNT(*) FROM users WHERE city = 'Delhi' OR age > 32"), 2);
    CHECK_EQ(count(e, "SELECT COUNT(*) FROM users WHERE NOT city = 'Pune'"), 2);
    CHECK_EQ(count(e, "SELECT COUNT(*) FROM users WHERE age IS NULL"), 1);
    CHECK_EQ(count(e, "SELECT COUNT(*) FROM users WHERE age > 0"), 3);          // NULL never matches
    CHECK_EQ(count(e, "SELECT COUNT(*) FROM users WHERE name LIKE '%a%'"), 2);  // Charlie, Dana
    CHECK_EQ(count(e, "SELECT COUNT(*) FROM users WHERE id BETWEEN 2 AND 3"), 2);
    CHECK_EQ(count(e, "SELECT COUNT(*) FROM users WHERE city IN ('Pune', 'Mumbai')"), 3);
    CHECK_EQ(count(e, "SELECT COUNT(*) FROM users WHERE (age + 5) * 2 = 70"), 1);
}

TEST(trailing_input_is_a_syntax_error) {
    Database db(mini_test::temp_dir("q_syntax"));
    QueryEngine e(db);
    seed(e);
    CHECK(err(e, "SELECT * FROM users WHERE id = 1 garbage").find("syntax error") != std::string::npos);
    CHECK(err(e, "SELEC 1").find("syntax error") != std::string::npos);
    CHECK(err(e, "SELECT 'oops").find("unterminated") != std::string::npos);
}

TEST(index_is_used_and_maintained) {
    Database db(mini_test::temp_dir("q_index"));
    QueryEngine e(db);
    seed(e);
    QueryResult r = ok(e, "SELECT name FROM users WHERE id = 2");
    CHECK(plan_has(r, "IndexScan"));
    CHECK_EQ(std::get<std::string>(r.rows[0][0]), std::string("Bob"));

    ok(e, "UPDATE users SET id = 7 WHERE id = 1");
    CHECK_EQ(count(e, "SELECT COUNT(*) FROM users WHERE id = 7"), 1);
    CHECK_EQ(count(e, "SELECT COUNT(*) FROM users WHERE id = 1"), 0);
    ok(e, "DELETE FROM users WHERE id = 7");
    CHECK_EQ(count(e, "SELECT COUNT(*) FROM users WHERE id = 7"), 0);

    // Secondary index built from existing rows, then kept current.
    ok(e, "CREATE INDEX ON orders (user_id)");
    CHECK_EQ(count(e, "SELECT COUNT(*) FROM orders WHERE user_id = 1"), 2);
    ok(e, "INSERT INTO orders VALUES (105, 1, 5.0)");
    CHECK_EQ(count(e, "SELECT COUNT(*) FROM orders WHERE user_id = 1"), 3);
    CHECK(plan_has(ok(e, "SELECT * FROM orders WHERE user_id >= 2"), "IndexScan"));
    CHECK(err(e, "CREATE INDEX ON users (name)").find("only supported on INT") != std::string::npos);
}

TEST(joins) {
    Database db(mini_test::temp_dir("q_join"));
    QueryEngine e(db);
    seed(e);
    QueryResult r = ok(e, "SELECT u.name, o.amount FROM users u JOIN orders o ON u.id = o.user_id ORDER BY o.amount DESC");
    CHECK(plan_has(r, "HashJoin"));
    CHECK_EQ(r.rows.size(), size_t(4));                    // order 104 has no matching user
    CHECK_EQ(std::get<std::string>(r.rows[0][0]), std::string("Charlie"));

    QueryResult n = ok(e, "SELECT u.name FROM users u JOIN orders o ON o.amount > u.age * 10");
    CHECK(plan_has(n, "NestedLoopJoin"));
    CHECK_EQ(count(e, "SELECT COUNT(*) FROM users, orders"), 20);    // cross join
    CHECK(err(e, "SELECT id FROM users a JOIN users b ON a.id = b.id").find("ambiguous") != std::string::npos);
}

TEST(group_by_having_order_limit_distinct) {
    Database db(mini_test::temp_dir("q_group"));
    QueryEngine e(db);
    seed(e);
    QueryResult r = ok(e, "SELECT city, COUNT(*) AS n, AVG(age) FROM users GROUP BY city HAVING COUNT(*) > 1");
    CHECK_EQ(r.rows.size(), size_t(1));
    CHECK_EQ(std::get<std::string>(r.rows[0][0]), std::string("Pune"));
    CHECK_EQ(std::get<double>(r.rows[0][2]), 32.5);

    QueryResult s = ok(e, "SELECT u.name, SUM(o.amount) AS total FROM users u JOIN orders o ON u.id = o.user_id "
                          "GROUP BY u.name ORDER BY total DESC LIMIT 1");
    CHECK_EQ(std::get<std::string>(s.rows[0][0]), std::string("Charlie"));

    CHECK_EQ(count(e, "SELECT COUNT(*) FROM users WHERE id > 100"), 0);   // empty input → one row
    CHECK_EQ(ok(e, "SELECT DISTINCT city FROM users").rows.size(), size_t(3));
    CHECK_EQ(count(e, "SELECT COUNT(DISTINCT city) FROM users"), 3);
    QueryResult o = ok(e, "SELECT name FROM users ORDER BY age DESC LIMIT 2");
    CHECK_EQ(std::get<std::string>(o.rows[0][0]), std::string("Charlie"));
    CHECK(err(e, "SELECT name, COUNT(*) FROM users").find("GROUP BY") != std::string::npos);
}

TEST(type_and_constraint_checks) {
    Database db(mini_test::temp_dir("q_types"));
    QueryEngine e(db);
    seed(e);
    CHECK(err(e, "INSERT INTO users VALUES ('oops', 'x', 1, 'y')").find("is INT") != std::string::npos);
    CHECK(err(e, "INSERT INTO users VALUES (1, 'Dup', 1, 'y')").find("duplicate primary key") != std::string::npos);
    CHECK(err(e, "INSERT INTO users (id) VALUES (50)").find("cannot be NULL") != std::string::npos);
    CHECK(err(e, "UPDATE users SET id = 2 WHERE id = 1").find("duplicate primary key") != std::string::npos);
    CHECK(err(e, "SELECT * FROM users WHERE name = 5").find("cannot compare") != std::string::npos);
    CHECK(err(e, "SELECT 1 / 0").find("division by zero") != std::string::npos);
    ok(e, "INSERT INTO orders VALUES (200, 1, 7)");                 // INT accepted into FLOAT
    QueryResult r = ok(e, "SELECT amount FROM orders WHERE oid = 200");
    CHECK(is_float(r.rows[0][0]));
}

TEST(data_survives_restart) {
    std::string dir = mini_test::temp_dir("q_restart");
    {
        Database db(dir);
        QueryEngine e(db);
        seed(e);
        ok(e, "CREATE INDEX ON orders (user_id)");
        for (int i = 0; i < 300; i++)
            ok(e, "INSERT INTO orders VALUES (" + std::to_string(1000 + i) + ", " + std::to_string(i % 10) + ", 1.5)");
    }
    Database db(dir);
    QueryEngine e(db);
    CHECK_EQ(db.catalog().table_count(), size_t(2));
    CHECK_EQ(count(e, "SELECT COUNT(*) FROM orders"), 305);
    QueryResult r = ok(e, "SELECT COUNT(*) FROM orders WHERE user_id = 3");
    CHECK(plan_has(r, "IndexScan"));
    CHECK_EQ(std::get<int64_t>(r.rows[0][0]), 31);                 // 30 new + order 103
}

TEST(drop_table_removes_files) {
    std::string dir = mini_test::temp_dir("q_drop");
    Database db(dir);
    QueryEngine e(db);
    seed(e);
    ok(e, "DROP TABLE users");
    CHECK(!std::filesystem::exists(dir + "/users.tbl"));
    CHECK(!std::filesystem::exists(dir + "/users.id.idx"));
    CHECK(err(e, "SELECT * FROM users").find("not found") != std::string::npos);
    ok(e, "CREATE TABLE users (id INT)");                           // the name can be reused, empty
    CHECK_EQ(count(e, "SELECT COUNT(*) FROM users"), 0);
}

TEST(script_stops_at_first_error) {
    Database db(mini_test::temp_dir("q_script"));
    QueryEngine e(db);
    auto results = e.execute_script("CREATE TABLE t (a INT); INSERT INTO t VALUES (1); "
                                    "INSERT INTO nope VALUES (2); INSERT INTO t VALUES (3);");
    CHECK_EQ(results.size(), size_t(3));
    CHECK(!results.back().ok);
    CHECK_EQ(count(e, "SELECT COUNT(*) FROM t"), 1);
}

TEST(explain_does_not_execute) {
    Database db(mini_test::temp_dir("q_explain"));
    QueryEngine e(db);
    seed(e);
    QueryResult r = ok(e, "EXPLAIN DELETE FROM users WHERE id = 1");
    CHECK(r.plan.has_value() && r.plan->rows == -1);
    CHECK_EQ(count(e, "SELECT COUNT(*) FROM users"), 4);
}

TEST_MAIN()
