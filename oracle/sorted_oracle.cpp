// oracle/sorted_oracle.cpp — see sorted_oracle.h. Owner: Oracle owner.
#include "sorted_oracle.h"

namespace oracle {

using eq::Event;

// Loop invariant: every index < lo holds a key > key; every index >= hi holds a key <= key.
std::size_t SortedOracle::lower(const eq::Key& key) {
    std::size_t lo = 0, hi = v_.size();
    while (lo < hi) {
        const std::size_t mid = lo + (hi - lo) / 2;
        EQ_COUNT(comparisons, 1);
        if (key < v_[mid].key) lo = mid + 1;   // v_[mid] is larger: answer is to the right
        else hi = mid;
    }
    return lo;
}

void SortedOracle::do_insert(const Event& e) {
    const std::size_t i = lower(e.key);
    EQ_COUNT(moves, v_.size() - i);
    v_.insert(v_.begin() + static_cast<std::ptrdiff_t>(i), e);
}

bool SortedOracle::cancel(const eq::Handle& h) {
    if (h.seq >= issued()) return false;
    const std::size_t i = lower(h);
    if (i >= v_.size() || v_[i].key != h) return false;   // only an exact key match cancels
    EQ_COUNT(moves, v_.size() - i - 1);
    v_.erase(v_.begin() + static_cast<std::ptrdiff_t>(i));
    return true;
}

bool SortedOracle::pop_min(Event& out) {
    if (v_.empty()) return false;
    out = v_.back();
    v_.pop_back();
    return true;
}

bool SortedOracle::peek_min(Event& out) {
    if (v_.empty()) return false;
    out = v_.back();
    return true;
}

// Entries 0..j-1 have time > t (they form a prefix, since the array is descending); the smallest
// of them is the last one, v_[j-1].
bool SortedOracle::successor(eq::Time t, Event& out) {
    std::size_t lo = 0, hi = v_.size();
    while (lo < hi) {
        const std::size_t mid = lo + (hi - lo) / 2;
        if (v_[mid].key.time > t) lo = mid + 1;
        else hi = mid;
    }
    if (lo == 0) return false;
    out = v_[lo - 1];
    return true;
}

}  // namespace oracle
