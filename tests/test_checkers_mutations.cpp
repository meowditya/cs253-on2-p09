// tests/test_checkers_mutations.cpp — proves the tests have teeth. Each "mutant" is a specific
// bug; the differential test or a checker must catch every one. Owner: Oracle owner.
//
// Behavioural mutants (run through the differential test against the oracle):
//   M1 ties broken LIFO instead of FIFO          M2 cancel ignores the handle's time
//   M3 successor uses >= instead of >             M4 no purge check after pop_min (checker: C <= L)
// Corrupted representations (fed straight to the checkers):
//   M5 stale AVL height     M6 duplicate heap entry + orphan table record
//   M7 bad AVL edges: out-of-range child, cycle, shared child, leaked pool slot
//   M8 a table record moved past an EMPTY slot on its probe path (records and counts unchanged,
//      but the table can no longer find it) — found by an external review; the checker must reject
//      it, and the real structure must indeed fail to cancel that event
#include <limits>
#include <memory>
#include <vector>

#include "avl_queue.h"
#include "checkers.h"
#include "difftest_core.h"
#include "lazy_heap.h"
#include "queue_factory.h"
#include "sorted_oracle.h"
#include "test_util.h"

using eq::Event;
using eq::Handle;

namespace {

// M1: a sorted-array queue whose tie rule is reversed (later seq first among equal times).
class LifoTies final : public eq::EventQueue {
public:
    bool cancel(const Handle& h) override {
        if (h.seq >= issued()) return false;
        for (std::size_t i = 0; i < v_.size(); ++i)
            if (v_[i].key == h) { v_.erase(v_.begin() + static_cast<long>(i)); return true; }
        return false;
    }
    bool pop_min(Event& out) override { if (v_.empty()) return false; out = v_.front(); v_.erase(v_.begin()); return true; }
    bool peek_min(Event& out) override { if (v_.empty()) return false; out = v_.front(); return true; }
    bool successor(eq::Time t, Event& out) override {
        for (const Event& e : v_) if (e.key.time > t) { out = e; return true; }
        return false;
    }
    std::size_t size() const override { return v_.size(); }
    const char* name() const override { return "lifo-ties"; }
protected:
    void do_insert(const Event& e) override {
        std::size_t i = 0;
        while (i < v_.size() && (v_[i].key.time < e.key.time || (v_[i].key.time == e.key.time && v_[i].key.seq > e.key.seq))) ++i;
        v_.insert(v_.begin() + static_cast<long>(i), e);
    }
private:
    std::vector<Event> v_;   // ascending by (time, -seq)
};

// Wraps a correct queue and breaks one operation.
class Wrap : public eq::EventQueue {
public:
    explicit Wrap(std::unique_ptr<eq::EventQueue> in) : in_(std::move(in)) {}
    bool cancel(const Handle& h) override { return in_->cancel(h); }
    bool pop_min(Event& out) override { return in_->pop_min(out); }
    bool peek_min(Event& out) override { return in_->peek_min(out); }
    bool successor(eq::Time t, Event& out) override { return in_->successor(t, out); }
    std::size_t size() const override { return in_->size(); }
    const char* name() const override { return in_->name(); }
protected:
    void do_insert(const Event& e) override { (void)in_->insert(e.key.time, e.type, e.data); }
    std::unique_ptr<eq::EventQueue> in_;
};
// M2: cancel matches on seq only (ignores time) — scans by peeking is impossible, so use the
// fact that the wrapped oracle's handles share seqs with ours: rebuild the handle with the
// original time of that seq.
class IgnoreCancelTime final : public Wrap {
public:
    using Wrap::Wrap;
    bool cancel(const Handle& h) override {
        if (h.seq >= times_.size()) return false;
        return in_->cancel(Handle{times_[h.seq], h.seq});
    }
protected:
    void do_insert(const Event& e) override { times_.push_back(e.key.time); Wrap::do_insert(e); }
private:
    std::vector<eq::Time> times_;
};
// M3: successor is not strict.
class NonStrictSuccessor final : public Wrap {
public:
    using Wrap::Wrap;
    bool successor(eq::Time t, Event& out) override {
        return t == std::numeric_limits<eq::Time>::min() ? in_->successor(t, out) : in_->successor(t - 1, out);
    }
};

bool caught(const std::vector<difftest::Factory>& qs) {
    difftest::GenConfig cfg;
    cfg.seed = 5;
    cfg.ops = 20000;
    cfg.max_live = 500;
    const auto ops = difftest::generate(cfg);
    return difftest::run(ops, qs, 1, 20000).failed;
}

}  // namespace

int main() {
    const difftest::Factory oracle = [] { return app::make_queue("oracle"); };

    // ---- behavioural mutants
    CHECK(caught({oracle, [] { return std::unique_ptr<eq::EventQueue>(new LifoTies()); }}));
    CHECK(caught({oracle, [] { return std::unique_ptr<eq::EventQueue>(new IgnoreCancelTime(app::make_queue("oracle"))); }}));
    CHECK(caught({oracle, [] { return std::unique_ptr<eq::EventQueue>(new NonStrictSuccessor(app::make_queue("oracle"))); }}));
    {   // M4 needs C to reach L, which random histories rarely do (cancelled entries near the front
        // surface quickly). The "drain" pattern forces it: 1000 inserts, cancel the 499 latest
        // (C = 499 <= L = 501, no purge), then pop the 500 earliest.
        std::vector<eq::Op> ops;
        for (int i = 0; i < 1000; ++i) { eq::Op op; op.kind = 'I'; op.time = i; ops.push_back(op); }
        for (int k = 501; k < 1000; ++k) { eq::Op op; op.kind = 'C'; op.k = static_cast<std::uint64_t>(k); ops.push_back(op); }
        for (int i = 0; i < 500; ++i) { eq::Op op; op.kind = 'P'; ops.push_back(op); }
        structA::LazyHeapOptions o;
        o.purge_after_pop = false;
        const auto mutant = difftest::run(ops, {oracle, [o] { return std::unique_ptr<eq::EventQueue>(new structA::LazyHeap(o)); }}, 1, ops.size());
        CHECK(mutant.failed && mutant.category == "CHECKER");
        const auto correct = difftest::run(ops, {oracle, [] { return app::make_queue("heap"); }}, 1, ops.size());
        CHECK(!correct.failed);
    }
    // sanity: the correct structures are NOT flagged by the same test
    CHECK(!caught({oracle, [] { return app::make_queue("heap"); }, [] { return app::make_queue("avl"); }}));

    // the shrinker produces a small literal reproducer for M2
    {
        const std::vector<difftest::Factory> qs = {oracle, [] { return std::unique_ptr<eq::EventQueue>(new IgnoreCancelTime(app::make_queue("oracle"))); }};
        difftest::GenConfig cfg;
        cfg.seed = 5;
        cfg.ops = 20000;
        cfg.max_live = 500;
        auto ops = difftest::generate(cfg);
        const auto f = difftest::run(ops, qs, 1, 20000);
        std::vector<eq::Op> prefix(ops.begin(), ops.begin() + static_cast<long>(f.index + 1));
        const auto shrunk = difftest::shrink(prefix, qs, f.category);
        const auto& small = shrunk.ops;
        CHECK(f.failed && shrunk.one_minimal && small.size() < prefix.size() && difftest::run(small, qs, 1, 10).failed);
        bool one_minimal = true;                       // deleting any single op makes it pass
        for (std::size_t i = 0; i < small.size(); ++i) {
            auto c = small;
            c.erase(c.begin() + static_cast<long>(i));
            const auto r = difftest::run(c, qs, 1, c.size());
            one_minimal = one_minimal && !(r.failed && r.category == f.category);
        }
        CHECK(one_minimal);
    }

    // ---- corrupted representations
    structB::AvlQueue avl;
    for (int i = 0; i < 64; ++i) avl.insert(i, 0, 0);
    const checkers::AvlView good = checkers::view_of(avl);
    CHECK(checkers::check_avl_view(good).empty());
    {   // M5 stale height
        auto v = good;
        v.pool[v.root].height = static_cast<std::int8_t>(v.pool[v.root].height + 1);
        CHECK(!checkers::check_avl_view(v).empty());
    }
    {   // M7a child index out of range
        auto v = good;
        v.pool[v.root].left = static_cast<std::uint32_t>(v.pool.size() + 5);
        CHECK(!checkers::check_avl_view(v).empty());
    }
    {   // M7b cycle: a leaf points back at the root (must be rejected, not loop forever)
        auto v = good;
        std::uint32_t x = v.root;
        while (v.pool[x].left) x = v.pool[x].left;
        v.pool[x].left = v.root;
        CHECK(!checkers::check_avl_view(v).empty());
    }
    {   // M7c shared child
        auto v = good;
        const std::uint32_t l = v.pool[v.root].left;
        v.pool[v.pool[v.root].right].left = l;
        CHECK(!checkers::check_avl_view(v).empty());
    }
    {   // M7d leaked slot: forget a free-list entry
        structB::AvlQueue a2;
        std::vector<Handle> h;
        for (int i = 0; i < 10; ++i) h.push_back(a2.insert(i, 0, 0));
        a2.cancel(h[3]);
        auto v = checkers::view_of(a2);
        CHECK(checkers::check_avl_view(v).empty());
        v.free_list.clear();
        CHECK(!checkers::check_avl_view(v).empty());
    }
    {   // M6 duplicate heap entry plus an orphan table record (the external review's case)
        structA::LazyHeap heap;
        for (int t : {10, 20, 30}) heap.insert(t, 0, 0);
        auto v = checkers::view_of(heap);
        CHECK(checkers::check_heap_view(v).empty());
        v.heap.push_back(v.heap[1]);
        std::size_t freeslot = 0;                                     // put an orphan record (seq 99,
        while (v.slots[freeslot].key != structA::SeqTable::kEmpty) ++freeslot;   // no heap entry) in a
        v.slots[freeslot] = checkers::TableSlot{99, 999, false, freeslot};       // free slot at its home
        v.live += 1;
        v.reported_size += 1;
        v.table_used += 1;
        CHECK(!checkers::check_heap_view(v).empty());
    }
    {   // M8 broken probe chain: same record set and counts, but the record sits behind an EMPTY slot
        structA::LazyHeap heap;
        const Handle h = heap.insert(10, 0, 0);
        auto v = checkers::view_of(heap);
        CHECK(checkers::check_heap_view(v).empty());
        const std::size_t cap = v.slots.size();
        std::size_t i = 0;
        while (v.slots[i].key != h.seq) ++i;
        const std::size_t j = (i + 1) & (cap - 1);
        v.slots[j] = v.slots[i];                  // record moved one slot on, home unchanged
        v.slots[i].key = structA::SeqTable::kEmpty;
        CHECK(!checkers::check_heap_view(v).empty());
        // the same corruption applied to a real (throw-away) structure: the event is still pending,
        // yet the table cannot find it — this is exactly what the check above protects against
        auto& t = const_cast<structA::SeqTable&>(heap.table());
        t.at(j) = t.at(i);
        t.at(i).key = structA::SeqTable::kEmpty;
        CHECK(!heap.cancel(h) && !checkers::check_heap_view(checkers::view_of(heap)).empty());
    }
    return testutil::finish("test_checkers_mutations");
}
