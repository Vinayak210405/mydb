#include "TestFramework.h"
#include "storage/Page.h"
#include "storage/HeapFile.h"
#include "storage/Tuple.h"
#include <string>

// Read a slot as a string (get() must run before len is used).
static std::string slot_str(const Page &p, uint16_t slot) {
    uint16_t len = 0;
    const char *d = p.get(slot, len);
    return d ? std::string(d, len) : std::string();
}

TEST(page_insert_get_remove) {
    Page p;
    const char *a = "hello", *b = "world!";
    int s0 = p.insert(a, 5), s1 = p.insert(b, 6);
    CHECK_EQ(s0, 0);
    CHECK_EQ(s1, 1);
    CHECK_EQ(slot_str(p, 1), std::string("world!"));
    CHECK(p.remove(0));
    uint16_t len = 0;
    CHECK(p.get(0, len) == nullptr);
    CHECK_EQ(p.live_count(), 1);
    // A deleted slot is reused by the next insert.
    CHECK_EQ(p.insert("x", 1), 0);
}

TEST(page_fills_up_then_compacts) {
    Page p;
    std::string row(100, 'r');
    int n = 0;
    while (p.insert(row.data(), 100) >= 0) n++;
    CHECK(n >= 38);                                  // (4096 - 16) / 104
    CHECK(!p.can_fit(100));
    // Free every other row: the space is fragmented, but compaction reclaims it.
    for (int i = 0; i < n; i += 2) p.remove(static_cast<uint16_t>(i));
    std::string big(150, 'b');
    CHECK(p.insert(big.data(), 150) >= 0);
    CHECK_EQ(slot_str(p, 1), row);  // survivors are intact after compaction
}

TEST(page_update_in_place_and_grow) {
    Page p;
    p.insert("abc", 3);
    p.insert("def", 3);
    CHECK(p.update(0, "zz", 2));
    CHECK(p.update(1, "a much longer value", 19));
    CHECK_EQ(slot_str(p, 0), std::string("zz"));
    CHECK_EQ(slot_str(p, 1), std::string("a much longer value"));
}

TEST(tuple_roundtrip) {
    std::vector<Value> row = {int64_t(-42), 3.5, std::string("it's"), true, std::monostate{}};
    auto bytes = Tuple::serialize(row);
    auto back = Tuple::deserialize(bytes.data(), bytes.size(), row.size());
    CHECK_EQ(back.size(), row.size());
    for (size_t i = 0; i < row.size(); i++) CHECK(values_equal(row[i], back[i]));
}

TEST(tuple_rejects_truncated_bytes) {
    auto bytes = Tuple::serialize({std::string("hello world")});
    CHECK_THROWS(Tuple::deserialize(bytes.data(), bytes.size() - 3, 1));
}

TEST(heapfile_persists_across_reopen) {
    std::string dir = mini_test::temp_dir("heap");
    std::string path = dir + "/t.tbl";
    std::vector<RID> rids;
    {
        HeapFile hf(path, 4);                         // tiny pool: forces evictions
        for (int i = 0; i < 2000; i++) {
            std::string s = "row-" + std::to_string(i);
            rids.push_back(hf.insert(s.data(), static_cast<uint16_t>(s.size())));
        }
        CHECK(hf.page_count() > 4);
        CHECK(hf.remove(rids[10]));
    }
    HeapFile hf(path, 4);
    CHECK_EQ(hf.row_count(), size_t(1999));
    std::vector<char> out;
    CHECK(hf.fetch(rids[1234], out));
    CHECK_EQ(std::string(out.begin(), out.end()), std::string("row-1234"));
    CHECK(!hf.fetch(rids[10], out));
}

TEST(heapfile_update_moves_row_when_page_is_full) {
    std::string dir = mini_test::temp_dir("heap_update");
    HeapFile hf(dir + "/t.tbl");
    std::string filler(1000, 'f');
    RID first = hf.insert(filler.data(), 1000);
    for (int i = 0; i < 3; i++) hf.insert(filler.data(), 1000);   // page 0 is now full
    std::string bigger(2000, 'g');
    RID moved = hf.update(first, bigger.data(), 2000);
    CHECK(moved != first);
    std::vector<char> out;
    CHECK(hf.fetch(moved, out));
    CHECK_EQ(out.size(), size_t(2000));
    CHECK(!hf.fetch(first, out));
}

TEST_MAIN()
