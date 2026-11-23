#include "TestFramework.h"
#include "index/BTree.h"
#include <map>
#include <random>

static RID rid_for(int i) { return RID{static_cast<PageID>(i), static_cast<uint16_t>(i % 97)}; }

TEST(btree_sequential_inserts_and_lookups) {
    std::string dir = mini_test::temp_dir("bt_seq");
    BTree t(dir + "/a.idx");
    for (int i = 0; i < 10000; i++) t.insert(i, rid_for(i));
    CHECK_EQ(t.entry_count(), size_t(10000));
    CHECK(t.height() >= 3);
    for (int i = 0; i < 10000; i += 7) {
        auto r = t.lookup(i);
        CHECK_EQ(r.size(), size_t(1));
        if (r.size() == 1) CHECK(r[0] == rid_for(i));
    }
    CHECK(t.lookup(-1).empty());
    CHECK(t.lookup(10000).empty());
}

TEST(btree_duplicates_across_leaf_splits) {
    // Heavy duplicates make equal keys land on both sides of a separator —
    // the case that needs the "leftmost leaf, then walk right" search.
    std::string dir = mini_test::temp_dir("bt_dup");
    BTree t(dir + "/a.idx", 8);
    std::multimap<int64_t, RID> ref;
    std::mt19937 rng(1);
    for (int i = 0; i < 20000; i++) {
        int64_t k = rng() % 300;
        t.insert(k, rid_for(i));
        ref.emplace(k, rid_for(i));
    }
    for (int64_t k = 0; k < 300; k++) CHECK_EQ(t.lookup(k).size(), ref.count(k));
}

TEST(btree_range_scan) {
    std::string dir = mini_test::temp_dir("bt_range");
    BTree t(dir + "/a.idx");
    for (int i = 0; i < 5000; i++) t.insert(i * 2, rid_for(i));       // even keys only
    CHECK_EQ(t.range(100, 199).size(), size_t(50));
    CHECK_EQ(t.range(-50, 3).size(), size_t(2));
    CHECK(t.range(20000, 30000).empty());
    CHECK(t.range(10, 5).empty());
}

TEST(btree_remove) {
    std::string dir = mini_test::temp_dir("bt_rm");
    BTree t(dir + "/a.idx");
    for (int i = 0; i < 3000; i++) t.insert(i % 1000, rid_for(i));
    CHECK(t.remove(5, rid_for(1005)));
    CHECK(!t.remove(5, rid_for(1005)));          // already gone
    CHECK(!t.remove(5000, rid_for(1)));          // never existed
    CHECK_EQ(t.lookup(5).size(), size_t(2));
    for (int i = 0; i < 3000; i++) t.remove(i % 1000, rid_for(i));
    CHECK_EQ(t.entry_count(), size_t(0));
    t.insert(7, rid_for(7));                     // still usable after emptying
    CHECK_EQ(t.lookup(7).size(), size_t(1));
}

TEST(btree_reopen_keeps_root) {
    std::string dir = mini_test::temp_dir("bt_reopen");
    PageID root;
    {
        BTree t(dir + "/a.idx", 8);
        for (int i = 0; i < 8000; i++) t.insert(i, rid_for(i));
        root = t.root();
        CHECK(root != 1);                        // the root has moved up from the first leaf
    }
    BTree t(dir + "/a.idx", 8);
    CHECK_EQ(t.root(), root);
    CHECK_EQ(t.lookup(4321).size(), size_t(1));
    CHECK_EQ(t.entry_count(), size_t(8000));
}

TEST(btree_rejects_non_index_file) {
    std::string dir = mini_test::temp_dir("bt_bad");
    {
        DiskManager d(dir + "/junk.idx");
        char page[PAGE_SIZE] = "not an index";
        d.allocate_page();
        d.write_page(0, page);
    }
    CHECK_THROWS(BTree(dir + "/junk.idx"));
}

TEST_MAIN()
