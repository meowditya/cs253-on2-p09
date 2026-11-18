// oracle/sorted_oracle.h — the reference queue: a sorted array, kept DESCENDING by key so the
// minimum is at the back. Obviously correct rather than fast. Owner: Oracle owner.
//
//   insert    : binary search (written here) + vector insert   O(log n) + O(n) shift
//   pop/peek  : back of the array                               O(1)
//   cancel    : binary search for the exact key + erase         O(n)
//   successor : binary search for the first time <= t           O(log n)
#pragma once
#include <cstddef>
#include <vector>

#include "event_queue.h"

namespace oracle {

class SortedOracle final : public eq::EventQueue {
public:
    bool cancel(const eq::Handle& h) override;
    bool pop_min(eq::Event& out) override;
    bool peek_min(eq::Event& out) override;
    bool successor(eq::Time t, eq::Event& out) override;
    std::size_t size() const override { return v_.size(); }
    const char* name() const override { return "oracle"; }

    const std::vector<eq::Event>& array() const { return v_; }   // for the checker

protected:
    void do_insert(const eq::Event& e) override;

private:
    // First index i with v_[i].key <= key (array is descending), or size() if none.
    std::size_t lower(const eq::Key& key);   // not const: it counts comparisons
    std::vector<eq::Event> v_;
};

}  // namespace oracle
