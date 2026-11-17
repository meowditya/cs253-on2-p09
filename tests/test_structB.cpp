// tests/test_structB.cpp — unit tests for Structure B (AVL tree with node pool).
// Owner: Structure B owner.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include "avl_queue.h"
#include "checkers.h"
#include "rng.h"
#include "test_util.h"

using eq::Event;
using eq::Handle;
using structB::AvlQueue;

static bool valid(const AvlQueue& q) { return checkers::check_avl_view(checkers::view_of(q)).empty(); }
static eq::Time root_time(const AvlQueue& q) { return q.pool()[q.root()].ev.key.time; }

// The four insertion cases on three keys: the middle key must end at the root.
static void test_rotation_cases() {
    const int orders[4][3] = {{3, 2, 1}, {1, 2, 3}, {3, 1, 2}, {1, 3, 2}};   // LL, RR, LR, RL
    const bool dbl[4] = {false, false, true, true};
    for (int c = 0; c < 4; ++c) {
        AvlQueue q;
        for (int k : orders[c]) q.insert(k, 0, 0);
        CHECK(root_time(q) == 2 && q.height() == 1 && valid(q));
        CHECK(q.rebalance_single() == (dbl[c] ? 0u : 1u));
        CHECK(q.rebalance_double() == (dbl[c] ? 1u : 0u));
    }
}

// After a deletion a child can have balance 0; then a SINGLE rotation is the correct fix.
static void test_delete_with_balanced_child() {
    AvlQueue q;
    std::vector<Handle> h;
    for (int k : {20, 10, 30, 5, 15, 40}) h.push_back(q.insert(k, 0, 0));  // left child 10 has 5 and 15
    CHECK(q.cancel(h[2]) && q.cancel(h[5]));                                 // remove 30 and 40
    CHECK(valid(q) && root_time(q) == 10 && q.size() == 4);
}

static void test_two_child_delete_and_order() {
    AvlQueue q;
    std::vector<Handle> h;
    for (int i = 0; i < 31; ++i) h.push_back(q.insert(i, 0, static_cast<std::uint32_t>(i)));
    CHECK(q.cancel(h[q.pool()[q.root()].ev.key.seq]));                       // root has two children
    CHECK(valid(q) && q.size() == 30);
    Event e{};
    eq::Time prev = -1;
    bool sorted = true;
    while (q.pop_min(e)) { sorted = sorted && e.key.time > prev && e.data == static_cast<std::uint32_t>(e.key.time); prev = e.key.time; }
    CHECK(sorted && valid(q));
}

static void test_edges() {
    AvlQueue q;
    const Event sentinel{eq::Key{-1, 1}, 9, 9};
    Event out = sentinel;
    CHECK(!q.pop_min(out) && !q.peek_min(out) && !q.successor(0, out) && out.data == 9);
    const Handle a = q.insert(5, 0, 1);
    q.insert(5, 0, 2);
    CHECK(!q.cancel(Handle{6, a.seq}) && !q.cancel(Handle{5, 77}));
    CHECK(!q.cancel(Handle{0, std::numeric_limits<eq::Seq>::max()}));
    CHECK(q.successor(4, out) && out.data == 1);
    out = sentinel;
    CHECK(!q.successor(5, out) && out.data == 9);
    CHECK(q.cancel(a) && !q.cancel(a));
    CHECK(q.pop_min(out) && out.data == 2 && !q.cancel(Handle{5, 1}) && q.empty() && valid(q));
}

// Ascending insertion (D5): rotation on every insert except k = 1, 2, 4, 8, ... (powers of two).
static void test_ascending_rotation_count() {
    AvlQueue q;
    const int n = 100000;
    std::uint64_t free_count = 0;
    bool only_powers = true;
    for (int k = 1; k <= n; ++k) {
        const auto before = q.rebalance_single() + q.rebalance_double();
        q.insert(k, 0, 0);
        if (q.rebalance_single() + q.rebalance_double() == before) {
            ++free_count;
            only_powers = only_powers && (k & (k - 1)) == 0;
        }
    }
    CHECK(free_count == 17 && only_powers);                       // floor(log2 1e5) + 1 = 17
    CHECK(q.rebalance_single() + q.rebalance_double() == static_cast<std::uint64_t>(n) - 17);
    CHECK(q.rebalance_double() == 0 && q.height() == 16 && valid(q));
}

// Pool growth while recursion is in progress, and free-list reuse.
static void test_pool_growth_and_reuse() {
    AvlQueue q;                                    // the pool starts tiny and reallocates many times
    eq::Rng r(3);
    std::vector<Handle> hs;
    for (int i = 0; i < 200000; ++i) hs.push_back(q.insert(static_cast<eq::Time>(r.below(1u << 30)), 0, 0));
    const std::size_t pool_after_fill = q.pool().size();
    Event e{};
    for (int i = 0; i < 100000; ++i) q.pop_min(e);
    for (int i = 0; i < 50000; ++i) q.cancel(hs[static_cast<std::size_t>(r.below(hs.size()))]);
    CHECK(valid(q));
    for (int i = 0; i < 150000; ++i) q.insert(static_cast<eq::Time>(r.below(1u << 30)), 0, 0);
    // freed slots are reused before the pool grows: the pool only exceeds its old size once the
    // free list is empty, and then holds exactly the live nodes plus NIL
    const std::size_t expect = std::max(pool_after_fill, q.size() + 1);
    CHECK(q.pool().size() == expect && valid(q));
}

static void test_height_bound_random() {
    AvlQueue q;
    eq::Rng r(9);
    for (int i = 0; i < 300000; ++i) q.insert(static_cast<eq::Time>(r.below(1u << 30)), 0, 0);
    // D2 with exact constants (not rounded down): h < log2(n+2)/log2(phi) + log_phi(sqrt 5) - 3
    const double phi = (1.0 + std::sqrt(5.0)) / 2.0;
    const double bound = std::log2(static_cast<double>(q.size()) + 2) / std::log2(phi) +
                         std::log(std::sqrt(5.0)) / std::log(phi) - 3.0;
    CHECK(q.height() < bound && valid(q));
}

int main() {
    test_rotation_cases();
    test_delete_with_balanced_child();
    test_two_child_delete_and_order();
    test_edges();
    test_ascending_rotation_count();
    test_pool_growth_and_reuse();
    test_height_bound_random();
    return testutil::finish("test_structB");
}
