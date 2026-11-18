// tests/test_structA.cpp — unit tests for Structure A (lazy heap + SeqTable).
// Owner: Structure A owner. Every scenario also runs the invariant checker.
#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

#include "checkers.h"
#include "lazy_heap.h"
#include "rng.h"
#include "test_util.h"

using eq::Event;
using eq::Handle;
using structA::LazyHeap;
using structA::LazyHeapOptions;
using structA::SeqTable;

static bool valid(const LazyHeap& q) { return checkers::check_heap_view(checkers::view_of(q)).empty(); }

static void test_table_reserved_keys() {
    SeqTable t;
    CHECK(t.find(SeqTable::kEmpty) == -1);      // the bug found in review: EMPTY must never match
    CHECK(t.find(SeqTable::kTomb) == -1);
    t.insert_new(5, 50);
    const long i = t.find(5);
    CHECK(i >= 0);
    t.erase_at(static_cast<std::size_t>(i));    // now a tombstone exists
    CHECK(t.find(SeqTable::kTomb) == -1);
    CHECK(t.find(SeqTable::kEmpty) == -1);
    CHECK(t.find(5) == -1);
}

static void test_table_growth_and_tombstones() {
    SeqTable t;
    bool healthy = true;
    for (std::uint64_t k = 0; k < 200000; ++k) {
        t.insert_new(k, static_cast<std::int64_t>(k) * 3);
        healthy = healthy && 2 * (t.used() + t.tombs()) <= t.capacity();
    }
    for (std::uint64_t k = 0; k < 200000; ++k) {
        const long i = t.find(k);
        if (i < 0 || t.at(static_cast<std::size_t>(i)).time != static_cast<std::int64_t>(k) * 3) { healthy = false; break; }
        t.erase_at(static_cast<std::size_t>(i));
    }
    CHECK(healthy);
    CHECK(t.used() == 0);
    // regrow through tombstones: same-size rebuilds must keep the half-full rule
    for (std::uint64_t k = 200000; k < 400000; ++k) {
        t.insert_new(k, 1);
        const long i = t.find(k - 1 >= 200000 ? k - 1 : k);
        if (k > 200000 && i >= 0) t.erase_at(static_cast<std::size_t>(i));
        healthy = healthy && 2 * (t.used() + t.tombs()) <= t.capacity();
    }
    CHECK(healthy);
}

static void test_ties_fifo() {
    LazyHeap q;
    for (std::uint32_t i = 0; i < 1000; ++i) q.insert(42, 0, i);
    Event e{};
    bool fifo = true;
    for (std::uint32_t i = 0; i < 1000; ++i) fifo = fifo && q.pop_min(e) && e.data == i;
    CHECK(fifo);
    CHECK(q.empty() && valid(q));
}

static void test_edge_cases() {
    LazyHeap q;
    const Event sentinel{eq::Key{-1, 1}, 9, 9};
    Event out = sentinel;
    CHECK(!q.pop_min(out) && out.data == 9);
    CHECK(!q.peek_min(out) && out.data == 9);
    CHECK(!q.successor(0, out) && out.data == 9);
    const Handle a = q.insert(10, 0, 1);
    q.insert(20, 0, 2);
    CHECK(!q.cancel(Handle{0, std::numeric_limits<eq::Seq>::max()}));        // reserved EMPTY
    CHECK(!q.cancel(Handle{0, std::numeric_limits<eq::Seq>::max() - 1}));    // reserved TOMB
    CHECK(!q.cancel(Handle{11, a.seq}));                                      // wrong time
    CHECK(!q.cancel(Handle{10, 99}));                                         // never issued
    CHECK(q.cancel(Handle{10, a.seq}));                                       // equal value cancels
    CHECK(!q.cancel(a));                                                      // twice
    CHECK(q.size() == 1 && valid(q));
    CHECK(q.pop_min(out) && out.data == 2);
    CHECK(q.empty() && valid(q));
    CHECK(q.physical() == 0);                                                 // last cancel/pop emptied it
    q.insert(std::numeric_limits<eq::Time>::max(), 0, 3);
    CHECK(!q.successor(std::numeric_limits<eq::Time>::max(), out));
    CHECK(q.successor(std::numeric_limits<eq::Time>::min(), out) && out.data == 3);
}

static void test_purge_triggers() {
    LazyHeap q;
    std::vector<Handle> h;
    for (int i = 0; i < 10; ++i) h.push_back(q.insert(i, 0, 0));
    for (int i = 0; i < 5; ++i) q.cancel(h[9 - i]);          // C = 5, L = 5: no purge yet
    CHECK(q.cancelled_in_heap() == 5 && q.physical() == 10 && q.purge_count() == 0);
    q.cancel(h[4]);                                           // C = 6 > L = 4: purge
    CHECK(q.purge_count() == 1 && q.cancelled_in_heap() == 0 && q.physical() == 4 && valid(q));
}

// The scenario showing the purge check must also follow pop_min (D1): cancel the 499 latest of
// 1000 events (C = 499 <= L = 501), then pop the 500 earliest. Without the post-pop check the
// array would hold 500 entries for 1 live event.
static void test_purge_after_pop_needed() {
    for (bool after_pop : {true, false}) {
        LazyHeapOptions o;
        o.purge_after_pop = after_pop;
        LazyHeap q(o);
        std::vector<Handle> h;
        for (int i = 0; i < 1000; ++i) h.push_back(q.insert(i, 0, 0));
        for (int i = 501; i < 1000; ++i) q.cancel(h[static_cast<std::size_t>(i)]);
        Event e{};
        for (int i = 0; i < 500; ++i) q.pop_min(e);
        if (after_pop) CHECK(q.size() == 1 && q.physical() <= 2 * q.size());
        else CHECK(q.size() == 1 && q.physical() == 500);
    }
}

static void test_random_vs_checker_and_successor() {
    LazyHeap q;
    eq::Rng r(11);
    std::vector<Handle> hs;
    std::vector<Event> live;   // brute-force model for successor
    bool ok = true;
    for (int step = 0; step < 60000 && ok; ++step) {
        const std::uint64_t x = r.below(10);
        if (x < 4) {
            const eq::Time t = static_cast<eq::Time>(r.below(500));
            const Handle hd = q.insert(t, 0, static_cast<std::uint32_t>(step));
            hs.push_back(hd);
            live.push_back(Event{hd, 0, static_cast<std::uint32_t>(step)});
        } else if (x < 7 && !hs.empty()) {
            const Handle hd = hs[static_cast<std::size_t>(r.below(hs.size()))];
            auto it = std::find_if(live.begin(), live.end(), [&](const Event& e) { return e.key == hd; });
            ok = ok && (q.cancel(hd) == (it != live.end()));
            if (it != live.end()) live.erase(it);
        } else if (x < 9) {
            Event e{};
            auto it = std::min_element(live.begin(), live.end(), [](const Event& a, const Event& b) { return a.key < b.key; });
            const bool got = q.pop_min(e);
            ok = ok && got == (it != live.end());
            if (got) { ok = ok && e.key == it->key; live.erase(it); }
        } else {
            const eq::Time t = static_cast<eq::Time>(r.below(520)) - 10;
            Event e{};
            const Event* best = nullptr;
            for (const Event& c : live) if (c.key.time > t && (!best || c.key < best->key)) best = &c;
            const bool got = q.successor(t, e);
            ok = ok && got == (best != nullptr) && (!got || e.key == best->key);
        }
        if (step % 97 == 0) ok = ok && valid(q);
    }
    CHECK(ok && valid(q));
}

static void test_two_lookups_per_hold() {
#if EQ_COUNTING
    LazyHeap q;
    for (int i = 0; i < 1000; ++i) q.insert(i * 7 % 1000, 0, 0);
    q.reset_counters();
    Event e{};
    for (int i = 0; i < 1000; ++i) { q.pop_min(e); q.insert(e.key.time + 500, 0, 0); }
    // one find for the root + one insertion probe sequence per hold; each is ~1 probe here
    CHECK(q.counters().hash_probes < 1000 * 3);
#endif
}

int main() {
    test_table_reserved_keys();
    test_table_growth_and_tombstones();
    test_ties_fifo();
    test_edge_cases();
    test_purge_triggers();
    test_purge_after_pop_needed();
    test_random_vs_checker_and_successor();
    test_two_lookups_per_hold();
    return testutil::finish("test_structA");
}
