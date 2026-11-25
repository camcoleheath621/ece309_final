// tests/final/test_final.cpp — YOUR TEST SUITE (spec §8).
//
// You need at least 20 tests. The 8 examples below are given and count:
//   Memory  (4): heap, lru, bst, ctx
//   Policy  (2): pol x2
//   Skills  (2): graph, index
// Write at least 12 more of your own, covering the list in spec §8.
// Name tests <area>_<what> with the areas heap, lru, bst, ctx, pol, graph,
// index, so the summary groups them.
//
//   ./build/test_final              run everything
//   ./build/test_final lru_         only tests whose names start with lru_
//   ./build/test_final =lru_evicts_least_recently_used    one test, exactly
//   ./build/test_final --list       list test names
//
// If a sanitizer error (ASan/UBSan) aborts the run, the tests after it do
// not run: rerun the failing test alone with =its_name and read the FIRST
// stack frame that is in your own file.
//
// CHECK_EQ(a, b) prints both values when it fails (vectors too), so prefer
// it over CHECK(a == b).
//
// Helpers you can use (tests/test_helpers.h): TempDir, write_text,
// write_skill, FixedPolicy, ScriptedConfirmer, LinesInput, StringOutput,
// parse_ok, args1. ManualClock is in policy/policy.h.

#include "../mini_test.h"
#include "../test_helpers.h"

#include "memory/context_builder.h"
#include "memory/memory_index.h"
#include "memory/memory_store.h"
#include "policy/policies.h"
#include "policy/policy_chain.h"
#include "skills/dep_graph.h"
#include "skills/skill_catalog.h"
#include "skills/skill_index.h"
#include "util/lru_cache.h"
#include "util/min_heap.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <cmath>
#include <random>
#include <algorithm>

// =========================================================================
// Memory — 4 examples
// =========================================================================

// Example 1 (heap): items come back smallest first, duplicates included.
TEST(heap_pops_in_order) {
    MinHeap<int> h;
    for (int x : {5, 1, 4, 1, 3}) h.push(x);
    CHECK_EQ(h.size(), std::size_t{5});
    CHECK_EQ(h.top(), 1);
    std::vector<int> got;
    while (!h.empty()) got.push_back(h.pop());
    CHECK_EQ(got, (std::vector<int>{1, 1, 3, 4, 5}));
}

// Example 2 (lru): the least recently USED entry is evicted, and get()
// counts as a use.
TEST(lru_evicts_least_recently_used) {
    LruCache<std::string, int> c(2);
    c.put("a", 1);
    c.put("b", 2);
    REQUIRE(c.get("a") != nullptr);   // "a" is now the most recent
    c.put("c", 3);                    // full: "b" must go
    CHECK(c.contains("a"));
    CHECK(!c.contains("b"));
    CHECK_EQ(c.keys_mru_to_lru(), (std::vector<std::string>{"c", "a"}));
    CHECK_EQ(c.stats().evictions, std::size_t{1});
}

// Example 3 (bst): removing a node with two children keeps the tree sorted,
// keeps every other record, and lowers size() by exactly one.
TEST(bst_remove_two_children) {
    MemoryIndex t;
    for (std::uint64_t id : {50, 30, 70, 60, 80}) REQUIRE(t.insert(MemoryRecord{id, "m" + std::to_string(id)}));
    REQUIRE(t.remove(70));
    std::vector<std::uint64_t> ids;
    t.for_each_in_order([&](const MemoryRecord& r) { ids.push_back(r.id); });
    CHECK_EQ(ids, (std::vector<std::uint64_t>{30, 50, 60, 80}));
    CHECK(t.find(70) == nullptr);
    REQUIRE(t.find(80) != nullptr);
    CHECK_EQ(t.find(80)->text, std::string("m80"));
    CHECK_EQ(t.size(), std::size_t{4});
}

// Example 4 (ctx): the worked example in spec §4.4.
TEST(ctx_spec_example) {
    std::vector<ContextItem> items{
        {Role::System, 10, false},     // 0  protected
        {Role::User, 10, false},       // 1
        {Role::Assistant, 10, false},  // 2  dropped 3rd (oldest assistant)
        {Role::Tool, 10, false},       // 3  dropped 1st
        {Role::Assistant, 10, false},  // 4
        {Role::Tool, 10, false},       // 5  dropped 2nd
        {Role::User, 10, false},       // 6  recent: protected
        {Role::Assistant, 10, false}}; // 7  recent: protected
    ContextPlan p = select_context(items, 50, 2);
    CHECK_EQ(p.keep, (std::vector<std::size_t>{0, 1, 4, 6, 7}));
    CHECK_EQ(p.total_tokens, std::size_t{50});
    CHECK(!p.over_budget);
}

// =========================================================================
// Policy — 2 examples
// =========================================================================

// Example 5 (pol): the sliding window. A request exactly `window` seconds
// after an allowed one no longer counts it.
TEST(pol_rate_limit_window_boundary) {
    ManualClock clock(100);
    RateLimitPolicy p(2, 10.0, clock);   // at most 2 per 10 s
    CommandRequest req;
    req.program = "ls";
    CHECK(p.evaluate(req).kind == Decision::Kind::Allow);   // t = 100
    clock.advance(1);
    CHECK(p.evaluate(req).kind == Decision::Kind::Allow);   // t = 101
    clock.advance(1);
    CHECK(p.evaluate(req).kind == Decision::Kind::Deny);    // t = 102: two in the window
    clock.advance(8);
    CHECK(p.evaluate(req).kind == Decision::Kind::Allow);   // t = 110: 100 has left (110 - 100 = 10)
    CHECK(p.tracked() <= 2u);
}

// Example 6 (pol): first Deny wins and later policies are not asked; with no
// Deny, the FIRST Ask is returned unchanged.
TEST(pol_chain_first_deny_stops_first_ask_wins) {
    auto ask1 = std::make_unique<FixedPolicy>(Decision::Kind::Ask, "ask1");
    auto ask2 = std::make_unique<FixedPolicy>(Decision::Kind::Ask, "ask2");
    PolicyChain chain;
    REQUIRE(chain.add(std::make_unique<FixedPolicy>(Decision::Kind::Allow, "ok")));
    REQUIRE(chain.add(std::move(ask1)));
    REQUIRE(chain.add(std::move(ask2)));
    CommandRequest req;
    req.program = "git";
    Decision d = chain.evaluate(req);
    CHECK(d.kind == Decision::Kind::Ask);
    CHECK_EQ(d.policy, std::string("ask1"));

    auto deny = std::make_unique<FixedPolicy>(Decision::Kind::Deny, "no");
    auto after = std::make_unique<FixedPolicy>(Decision::Kind::Allow, "after");
    FixedPolicy* after_ptr = after.get();   // still valid: the chain owns it now
    REQUIRE(chain.add(std::move(deny)));
    REQUIRE(chain.add(std::move(after)));
    d = chain.evaluate(req);
    CHECK(d.kind == Decision::Kind::Deny);
    CHECK_EQ(d.policy, std::string("no"));
    CHECK_EQ(after_ptr->calls, 0);     // never evaluated: the chain stopped at the Deny
}

// =========================================================================
// Skills — 2 examples
// =========================================================================

// Example 7 (graph): post-order load order, and a cycle message that lists
// only the nodes still on the DFS path (b is already finished).
TEST(graph_order_and_cycle_message) {
    DepGraph g;
    for (const char* n : {"a", "b", "c", "d"}) g.add_node(n);
    g.add_edge("a", "b");   // a requires b
    g.add_edge("a", "c");
    g.add_edge("b", "d");
    g.add_edge("c", "d");   // diamond: d is reached twice, but it is not a cycle
    std::vector<std::string> order;
    std::string error;
    REQUIRE(g.order_from("a", &order, &error));
    CHECK_EQ(order, (std::vector<std::string>{"d", "b", "c", "a"}));

    g.add_edge("c", "a");   // now a -> c -> a is a cycle
    CHECK(!g.order_from("a", &order, &error));
    CHECK_EQ(error, std::string("cycle: a -> c -> a"));
}

// Example 8 (index): rank by number of distinct query words; ties go to the
// smaller doc id.
TEST(index_ranking_and_ties) {
    SkillIndex ix;
    ix.add(0, "format numbers");
    ix.add(1, "csv report with table and numbers");
    ix.add(2, "table format");
    ix.add(3, "unrelated");
    CHECK_EQ(ix.search("csv table numbers", 10), (std::vector<std::size_t>{1, 0, 2}));
    CHECK_EQ(ix.search("format table", 1), (std::vector<std::size_t>{2}));
    REQUIRE(ix.postings("numbers") != nullptr);
    CHECK_EQ(*ix.postings("numbers"), (std::vector<std::size_t>{0, 1}));
}

// =========================================================================
// Your tests (at least 12 more). Spec §8 lists what they must cover.
// =========================================================================

// Test 9: Check AVL height after sequential insertions and random removals
TEST(bst_avl_large_workload) {
    MemoryIndex tree; // Create an empty tree

    constexpr std::uint64_t record_count = 10000; // Number of records to test
    const auto max_height = static_cast<std::size_t>(
        std::ceil(1.45 * std::log2(record_count + 2))); // Conservative AVL height limit

    // Insert IDs from 1 through 10000
    for (std:: uint64_t id = 1; id <= record_count; ++id) {
        REQUIRE(tree.insert(MemoryRecord{id, "m" + std::to_string(id)}));
    }

    // Check the size and height after insertion
    CHECK_EQ(tree.size(), std::size_t{record_count});
    CHECK(tree.height() <= max_height);

    // Verify the stored records
    for (std::uint64_t id = 1; id <= record_count; ++id) {
        const MemoryRecord* rec = tree.find(id);        // lookup current ID
        REQUIRE(rec != nullptr);    // stop test if record is missing
        CHECK_EQ(rec->id, id);      // check the stored ID
        CHECK_EQ(rec->text, "m" + std::to_string(id));  // check the stored text
    }

    // Create a vector holding the IDs to remove
    std::vector<std::uint64_t> removal_ids;

    // Fill vector with all ID's
    for(std::uint64_t id = 1; id <= record_count; ++id) {
        removal_ids.push_back(id);
    }

    // Create a random number generator with a fixed seed
    constexpr std::mt19937::result_type random_seed = 309;  // Seed for repeatable shuffling
    std::mt19937 generator(random_seed);                    // Initialize the generator

    // Shuffle those IDs
    std::shuffle(removal_ids.begin(), removal_ids.end(), generator);

    // Track expected number of records
    std::size_t remaining = tree.size();

    // Remove each ID in the shuffled order
    for(std::uint64_t id : removal_ids) {
        REQUIRE(tree.remove(id));   // require removal to succeed
        --remaining;                // update the expected size

        // Calculate the bound for the remaining records
        const auto height_limit = static_cast<std::size_t>(
        std::ceil(1.45 * std::log2(remaining + 2))); 
        
        // Verify the tree after the removal
        CHECK(tree.find(id) == nullptr);        // verify this ID is gone
        CHECK_EQ(tree.size(), remaining);       // verify the size matches the remaining count
        CHECK(tree.height() <= height_limit);   // verify that height is within the AVL bound
    }
    
    // Verify all the records have been removed and tree is empty
    CHECK_EQ(tree.size(), std::size_t{0});      
    CHECK_EQ(tree.height(), std::size_t{0});    
}

// Test 10: reject a duplicate ID without changing the original record
TEST(bst_duplicate_id) {
    MemoryIndex tree;   // create an empty tree
    
    std::uint64_t id = 123;         // use the same id 
    std::string og_message  = "m";  // original cached text
    std::string new_message = "n";  // different text for duplicate

    // Insert the original record and reject the duplicate
    REQUIRE(tree.insert(MemoryRecord{id, og_message}));
    CHECK(!tree.insert(MemoryRecord{id, new_message}));
    CHECK_EQ(tree.size(), std:: size_t{1}); // verify that size is the same
    
    // Verify that the original record is unchanged
    const MemoryRecord* rec = tree.find(id);
    REQUIRE(rec != nullptr);
    CHECK_EQ(rec->id, id);
    CHECK_EQ(rec->text, og_message);
}

// Test 11: verify a right rotation fixes the LL insertion case
TEST(bst_avl_rotation_LL){
    MemoryIndex tree;   // create an empty tree

    // Define the ids and their original text
    std::uint64_t id_30 = 30;
    std::uint64_t id_20 = 20;
    std::uint64_t id_10 = 10;
    std::string message_30 = "m30";
    std::string message_20 = "m20";
    std::string message_10 = "m10";

    //Insert in descending order to trigger the LL case
    REQUIRE(tree.insert(MemoryRecord{id_30,message_30}));
    REQUIRE(tree.insert(MemoryRecord{id_20,message_20}));
    REQUIRE(tree.insert(MemoryRecord{id_10,message_10}));

    // Verify LL rotation preserves size and restores balance
    CHECK_EQ(tree.size(), std:: size_t{3});
    CHECK_EQ(tree.height(), std:: size_t{2});

    std::vector<std::uint64_t> ids; // store ids in order visited

    // Collect each record'd ID during in-order transversal
    tree.for_each_in_order([&](const MemoryRecord& rec) {
        ids.push_back(rec.id);  // append the current record's id
    });

    CHECK_EQ(ids, (std::vector<std::uint64_t>{10,20,30})); // verify the sorted order
    
    // Verify every record has kept its original ID and text
    for(std::uint64_t id : {10,20,30}) {
        const MemoryRecord* rec = tree.find(id);        // lookup id
        REQUIRE(rec != nullptr);                        // stop if record is missing
        CHECK_EQ(rec->id, id);                          // verify the stored id
        CHECK_EQ(rec->text, "m" + std::to_string(id));  // verify the original text
    } 

}

// Test 12: verify a left-right double rotation fixes the LR insertion case
TEST(bst_avl_rotation_LR){
    MemoryIndex tree;   // create an empty tree

    // Define the ids and their original text
    std::uint64_t id_30 = 30;
    std::uint64_t id_20 = 20;
    std::uint64_t id_10 = 10;
    std::string message_30 = "m30";
    std::string message_20 = "m20";
    std::string message_10 = "m10";

    //Insert in 30, 10, 20 to trigger the LR case
    REQUIRE(tree.insert(MemoryRecord{id_30,message_30}));
    REQUIRE(tree.insert(MemoryRecord{id_10,message_10}));
    REQUIRE(tree.insert(MemoryRecord{id_20,message_20}));

    // Verify LR rotation preserves size and restores balance
    CHECK_EQ(tree.size(), std:: size_t{3});
    CHECK_EQ(tree.height(), std:: size_t{2});

    std::vector<std::uint64_t> ids; // store ids in order visited

    // Collect each record'd ID during in-order transversal
    tree.for_each_in_order([&](const MemoryRecord& rec) {
        ids.push_back(rec.id);  // append the current record's id
    });

    CHECK_EQ(ids, (std::vector<std::uint64_t>{10,20,30})); // verify the sorted order
    
    // Verify every record has kept its original ID and text
    for(std::uint64_t id : {10,20,30}) {
        const MemoryRecord* rec = tree.find(id);        // lookup id
        REQUIRE(rec != nullptr);                        // stop if record is missing
        CHECK_EQ(rec->id, id);                          // verify the stored id
        CHECK_EQ(rec->text, "m" + std::to_string(id));  // verify the original text
    } 

}

// Test 13: verify a left rotation fixes the RR insertion case
TEST(bst_avl_rotation_RR){
    MemoryIndex tree;   // create an empty tree

    // Define the ids and their original text
    std::uint64_t id_30 = 30;
    std::uint64_t id_20 = 20;
    std::uint64_t id_10 = 10;
    std::string message_30 = "m30";
    std::string message_20 = "m20";
    std::string message_10 = "m10";

    //Insert in ascending order to trigger the RR case
    REQUIRE(tree.insert(MemoryRecord{id_10,message_10}));
    REQUIRE(tree.insert(MemoryRecord{id_20,message_20}));
    REQUIRE(tree.insert(MemoryRecord{id_30,message_30}));

    // Verify RR rotation preserves size and restores balance
    CHECK_EQ(tree.size(), std:: size_t{3});
    CHECK_EQ(tree.height(), std:: size_t{2});

    std::vector<std::uint64_t> ids; // store ids in order visited

    // Collect each record'd ID during in-order transversal
    tree.for_each_in_order([&](const MemoryRecord& rec) {
        ids.push_back(rec.id);  // append the current record's id
    });

    CHECK_EQ(ids, (std::vector<std::uint64_t>{10,20,30})); // verify the sorted order
    
    // Verify every record has kept its original ID and text
    for(std::uint64_t id : {10,20,30}) {
        const MemoryRecord* rec = tree.find(id);        // lookup id
        REQUIRE(rec != nullptr);                        // stop if record is missing
        CHECK_EQ(rec->id, id);                          // verify the stored id
        CHECK_EQ(rec->text, "m" + std::to_string(id));  // verify the original text
    } 

}

// Test 14: verify a right-left double rotation fixes the RL insertion case
TEST(bst_avl_rotation_RL){
    MemoryIndex tree;   // create an empty tree

    // Define the ids and their original text
    std::uint64_t id_30 = 30;
    std::uint64_t id_20 = 20;
    std::uint64_t id_10 = 10;
    std::string message_30 = "m30";
    std::string message_20 = "m20";
    std::string message_10 = "m10";

    //Insert in 10, 30, 20 to trigger the RL case
    REQUIRE(tree.insert(MemoryRecord{id_10,message_10}));
    REQUIRE(tree.insert(MemoryRecord{id_30,message_30}));
    REQUIRE(tree.insert(MemoryRecord{id_20,message_20}));

    // Verify RL rotation preserves size and restores balance
    CHECK_EQ(tree.size(), std:: size_t{3});
    CHECK_EQ(tree.height(), std:: size_t{2});

    std::vector<std::uint64_t> ids; // store ids in order visited

    // Collect each record'd ID during in-order transversal
    tree.for_each_in_order([&](const MemoryRecord& rec) {
        ids.push_back(rec.id);  // append the current record's id
    });

    CHECK_EQ(ids, (std::vector<std::uint64_t>{10,20,30})); // verify the sorted order
    
    // Verify every record has kept its original ID and text
    for(std::uint64_t id : {10,20,30}) {
        const MemoryRecord* rec = tree.find(id);        // lookup id
        REQUIRE(rec != nullptr);                        // stop if record is missing
        CHECK_EQ(rec->id, id);                          // verify the stored id
        CHECK_EQ(rec->text, "m" + std::to_string(id));  // verify the original text
    } 

}

int main(int argc, char** argv) { return mt::run_all(argc, argv); }
