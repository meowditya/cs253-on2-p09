// src/structA/lazy_heap.h — Structure A: binary min-heap keyed by (time, seq) with lazy
// cancellation. Owner: Structure A owner. Derivation: D1.
//
// cancel() only flags the event in a hash table (SeqTable). Flagged ("stale") entries stay in the
// heap array until they reach the root, where pop_min/peek_min discard them, or until a purge
// rebuilds the array without them. A purge runs whenever cancelled entries outnumber live ones,
// checked after every cancel AND every pop_min, so the array never holds more than 2 x live
// entries (D1). Checking only after cancels is unbounded: see tests/test_structA.cpp.
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

#include "event_queue.h"
#include "seq_table.h"

namespace structA {

struct LazyHeapOptions {
    bool purge = true;              // false only for the adversary's "no purge" experiment
    bool purge_after_pop = true;    // false only for the mutation test that proves it is needed
};

class LazyHeap final : public eq::EventQueue {
public:
    explicit LazyHeap(LazyHeapOptions options = {});

    bool cancel(const eq::Handle& h) override;
    bool pop_min(eq::Event& out) override;
    bool peek_min(eq::Event& out) override;
    bool successor(eq::Time t, eq::Event& out) override;
    std::size_t size() const override { return live_; }
    const char* name() const override { return options_.purge ? "heap" : "heap-nopurge"; }

    // ---- read-only inspection (checkers, experiments) ----
    const std::vector<eq::Event>& heap_array() const { return a_; }
    const SeqTable& table() const { return table_; }
    std::size_t live() const { return live_; }
    std::size_t cancelled_in_heap() const { return cancelled_; }
    std::size_t physical() const { return a_.size(); }
    std::size_t peak_physical() const { return peak_physical_; }
    void reset_peak_physical() { peak_physical_ = a_.size(); }   // e.g. at the start of a timed region
    std::uint64_t purge_count() const { return purges_; }
    std::uint64_t purged_entries() const { return purged_entries_; }
    std::uint64_t stale_roots_discarded() const { return stale_discards_; }
    std::uint64_t table_rebuilds() const { return table_.rebuilds(); }
    std::uint64_t table_rebuild_slots() const { return table_.rebuild_slot_work(); }
    const LazyHeapOptions& options() const { return options_; }
    std::size_t allocated_bytes() const {
        return a_.capacity() * sizeof(eq::Event) + table_.capacity() * sizeof(SeqTable::Slot);
    }

protected:
    void do_insert(const eq::Event& e) override;

private:
    bool less(const eq::Event& x, const eq::Event& y) {
        EQ_COUNT(comparisons, 1);
        return x.key < y.key;
    }
    void sift_up(std::size_t i);
    void sift_down(std::size_t i);
    eq::Event remove_root();          // physical removal; does not look at the table
    void discard_stale_roots();
    void maybe_purge() { if (options_.purge && cancelled_ > live_) purge(); }
    void purge();

    LazyHeapOptions options_;
    std::vector<eq::Event> a_;        // the heap; a_[0] is the minimum entry (live or stale)
    SeqTable table_;
    std::size_t live_ = 0;            // L
    std::size_t cancelled_ = 0;       // C
    std::size_t peak_physical_ = 0;
    std::uint64_t purges_ = 0;
    std::uint64_t purged_entries_ = 0;
    std::uint64_t stale_discards_ = 0;
    std::vector<std::size_t> stack_;  // reused by successor()
};

}  // namespace structA
