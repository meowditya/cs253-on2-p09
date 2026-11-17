// src/structA/lazy_heap.cpp — see lazy_heap.h. Owner: Structure A owner.
#include "lazy_heap.h"

namespace structA {

using eq::Event;

LazyHeap::LazyHeap(LazyHeapOptions options)
    : options_(options), table_(&counters_.hash_probes, &counters_.hash_rehash_probes) {}

// Hole method: lift the new element past larger parents without swapping.
void LazyHeap::sift_up(std::size_t i) {
    const Event x = a_[i];
    while (i > 0) {
        const std::size_t p = (i - 1) / 2;
        if (!less(x, a_[p])) break;
        a_[i] = a_[p];
        EQ_COUNT(moves, 1);
        i = p;
    }
    a_[i] = x;
}

// Hole method: move the smaller child up until x fits.
void LazyHeap::sift_down(std::size_t i) {
    const std::size_t n = a_.size();
    const Event x = a_[i];
    for (;;) {
        std::size_t c = 2 * i + 1;
        if (c >= n) break;
        if (c + 1 < n && less(a_[c + 1], a_[c])) ++c;
        if (!less(a_[c], x)) break;
        a_[i] = a_[c];
        EQ_COUNT(moves, 1);
        i = c;
    }
    a_[i] = x;
}

Event LazyHeap::remove_root() {
    const Event top = a_[0];
    const Event last = a_.back();
    a_.pop_back();
    if (!a_.empty()) {
        a_[0] = last;
        sift_down(0);
    }
    return top;
}

void LazyHeap::do_insert(const Event& e) {
    a_.push_back(e);
    sift_up(a_.size() - 1);
    table_.insert_new(e.key.seq, e.key.time);
    ++live_;
    if (a_.size() > peak_physical_) peak_physical_ = a_.size();
}

void LazyHeap::discard_stale_roots() {
    while (!a_.empty()) {
        const long i = table_.find(a_[0].key.seq);   // every array entry has a record (invariant)
        if (!table_.at(static_cast<std::size_t>(i)).cancelled) return;
        remove_root();
        table_.erase_at(static_cast<std::size_t>(i));
        --cancelled_;
        ++stale_discards_;
    }
}

bool LazyHeap::cancel(const eq::Handle& h) {
    if (h.seq >= issued()) return false;              // never issued (also rejects reserved values)
    const long i = table_.find(h.seq);
    if (i < 0) return false;                           // already popped or discarded
    SeqTable::Slot& s = table_.at(static_cast<std::size_t>(i));
    if (s.time != h.time || s.cancelled) return false; // forged time, or cancelled already
    s.cancelled = 1;
    --live_;
    ++cancelled_;
    maybe_purge();
    return true;
}

bool LazyHeap::pop_min(Event& out) {
    // Look at the root once; reuse that lookup to erase it if it is live (2 lookups per hold).
    while (!a_.empty()) {
        const std::size_t i = static_cast<std::size_t>(table_.find(a_[0].key.seq));
        const bool stale = table_.at(i).cancelled != 0;
        const Event top = remove_root();               // the table is untouched, so i stays valid
        table_.erase_at(i);
        if (stale) {
            --cancelled_;
            ++stale_discards_;
            continue;
        }
        out = top;
        --live_;
        if (options_.purge_after_pop) maybe_purge();   // live just fell: C > L is possible now
        return true;
    }
    return false;
}

bool LazyHeap::peek_min(Event& out) {
    discard_stale_roots();                             // only lowers C, so no purge check needed
    if (a_.empty()) return false;
    out = a_[0];
    return true;
}

// Rebuild the array without cancelled entries: compact with a write pointer, then Floyd.
void LazyHeap::purge() {
    const std::size_t m = a_.size();
    std::size_t w = 0;
    for (std::size_t r = 0; r < m; ++r) {
        const std::size_t i = static_cast<std::size_t>(table_.find(a_[r].key.seq));
        if (table_.at(i).cancelled) {
            table_.erase_at(i);
        } else {
            a_[w++] = a_[r];
        }
    }
    a_.resize(w);
    purged_entries_ += m - w;
    ++purges_;
    EQ_COUNT(purges, 1);
    EQ_COUNT(purged_entries, m - w);
    cancelled_ = 0;
    for (std::size_t i = w / 2; i > 0;) sift_down(--i);   // Floyd build-heap, O(w)
}

// Smallest live key with time > t. Pruned depth-first search over the implicit tree: a subtree
// can be skipped once its root is not smaller than the best found, because heap order makes every
// descendant >= its root. Worst case visits every entry (O(m)).
bool LazyHeap::successor(eq::Time t, Event& out) {
    const std::size_t n = a_.size();
    bool found = false;
    Event best{};
    stack_.clear();
    if (n > 0) stack_.push_back(0);
    while (!stack_.empty()) {
        const std::size_t i = stack_.back();
        stack_.pop_back();
        const Event& e = a_[i];
        if (found && !less(e, best)) continue;                       // whole subtree >= best
        if (e.key.time > t) {
            const long s = table_.find(e.key.seq);
            if (!table_.at(static_cast<std::size_t>(s)).cancelled) {   // live candidate:
                best = e;                                              // its descendants are larger
                found = true;
                continue;
            }
        }
        const std::size_t c = 2 * i + 1;                              // too early or stale: look below
        if (c < n) stack_.push_back(c);
        if (c + 1 < n) stack_.push_back(c + 1);
    }
    if (found) out = best;
    return found;
}

}  // namespace structA
