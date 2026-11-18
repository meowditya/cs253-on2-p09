// tests/test_fullscale.cpp — the handout's scale requirement as a CORRECTNESS test: at least 10^6
// events resident at once and 10^6 operations, on all three queues. Owner: Oracle owner.
//
// 1. 10^6 inserts with DESCENDING times (so the descending-array oracle appends in O(1)),
//    checker once at full size, then 10^6 pops that must come out in time order with payloads.
// 2. The same with ties (blocks of 10 equal times), checking FIFO order inside each block.
// 3. Heap vs AVL on a mixed workload with exactly 10^6 live events throughout: 10^6 operations
//    (hold; cancel + replacement insert only when the cancel succeeded; peek; ~100 successor
//    queries), every returned Event compared field by field.
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "checkers.h"
#include "queue_factory.h"
#include "rng.h"
#include "test_util.h"

using eq::Event;
using eq::Handle;

static const std::uint32_t N = 1000000;

static void descending_fill_and_drain(const std::string& name, std::uint32_t block) {
    auto q = app::make_queue(name);
    for (std::uint32_t i = 0; i < N; ++i) q->insert(static_cast<eq::Time>((N - 1 - i) / block), 0, i);
    CHECK(q->size() == N);
    const std::string msg = checkers::check_queue(*q);
    testutil::check(msg.empty(), name + " checker at 10^6 live: " + msg);
    Event e{};
    bool ordered = true;
    eq::Time prev_t = -1;
    eq::Seq prev_s = 0;
    for (std::uint32_t i = 0; i < N; ++i) {
        if (!q->pop_min(e)) { ordered = false; break; }
        const bool time_ok = e.key.time > prev_t || (e.key.time == prev_t && e.key.seq > prev_s);
        const bool payload_ok = e.data == e.key.seq;                         // data was the insert index
        const bool fifo_ok = (N - 1 - e.data) / block == static_cast<std::uint32_t>(e.key.time);
        ordered = ordered && time_ok && payload_ok && fifo_ok;
        prev_t = e.key.time;
        prev_s = e.key.seq;
    }
    testutil::check(ordered && q->empty() && checkers::check_queue(*q).empty(),
                    name + " drained 10^6 in order (block " + std::to_string(block) + ")");
}

static void heap_vs_avl_million_live() {
    auto h = app::make_queue("heap"), a = app::make_queue("avl");
    eq::Rng r(2026);
    std::vector<Handle> hs;
    hs.reserve(2 * N);
    for (std::uint32_t i = 0; i < N; ++i) {
        const eq::Time t = static_cast<eq::Time>(r.below(1u << 30));
        const Handle x = h->insert(t, 0, i), y = a->insert(t, 0, i);
        if (!(x == y)) { CHECK(false); return; }
        hs.push_back(x);
    }
    bool same = h->size() == N && a->size() == N;
    std::uint64_t cancels_ok = 0;
    for (std::uint32_t i = 0; i < N && same; ++i) {
        const std::uint64_t op = r.below(10);
        Event e1{}, e2{};
        if (op < 4) {
            const bool g1 = h->pop_min(e1), g2 = a->pop_min(e2);
            same = g1 == g2 && (!g1 || (e1.key == e2.key && e1.type == e2.type && e1.data == e2.data));
            if (g1) {                                              // hold: re-insert after the popped time
                const eq::Time t = e1.key.time + 1 + static_cast<eq::Time>(r.below(1u << 20));
                hs.push_back(h->insert(t, 1, i));
                a->insert(t, 1, i);
            }
        } else if (op < 7) {
            const Handle k = hs[static_cast<std::size_t>(r.below(hs.size()))];
            const bool c1 = h->cancel(k), c2 = a->cancel(k);
            same = c1 == c2;
            if (c1) {                                              // replace only what was removed, so
                ++cancels_ok;                                      // the live size stays exactly 10^6
                const eq::Time t = static_cast<eq::Time>(r.below(1u << 30));
                hs.push_back(h->insert(t, 2, i));
                a->insert(t, 2, i);
            }
        } else if (r.below(3000) == 0) {                          // ~100 queries: the heap's successor
            const eq::Time t = static_cast<eq::Time>(r.below(1u << 30));   // is O(n) by design
            const bool g1 = h->successor(t, e1), g2 = a->successor(t, e2);
            same = g1 == g2 && (!g1 || (e1.key == e2.key && e1.type == e2.type && e1.data == e2.data));
        } else {
            const bool g1 = h->peek_min(e1), g2 = a->peek_min(e2);
            same = g1 == g2 && (!g1 || (e1.key == e2.key && e1.type == e2.type && e1.data == e2.data));
        }
        same = same && h->size() == N && a->size() == N;
    }
    CHECK(same && cancels_ok > 0);
    const std::string mh = checkers::check_queue(*h), ma = checkers::check_queue(*a);
    testutil::check(mh.empty() && ma.empty(), "checkers after the million-live workload: " + mh + ma);
}

int main() {
    for (const char* q : {"heap", "avl", "oracle"}) {
        descending_fill_and_drain(q, 1);
        descending_fill_and_drain(q, 10);
    }
    heap_vs_avl_million_live();
    return testutil::finish("test_fullscale");
}
