// src/structB/avl_queue.h — Structure B: AVL tree keyed by (time, seq), used as a priority queue
// with true deletion. Owner: Structure B owner. Derivation: D2.
//
// Nodes live in a vector "pool" and refer to each other by 32-bit index; index 0 is the NIL
// sentinel with height -1 (a leaf has height 0, matching D2). Deleted nodes go on a free list and
// are reused first. IMPORTANT: a recursive insert may grow the pool vector, which moves it in
// memory, so no reference/pointer into the pool is ever kept across a call that can allocate —
// only indices, re-read as pool_[x] after the call.
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

#include "event_queue.h"

namespace structB {

struct AvlNode {
    eq::Event     ev;
    std::uint32_t left;
    std::uint32_t right;
    std::int8_t   height;   // NIL = -1, leaf = 0; at most 27 for 10^6 nodes (D2)
};

class AvlQueue final : public eq::EventQueue {
public:
    AvlQueue();

    bool cancel(const eq::Handle& h) override;
    bool pop_min(eq::Event& out) override;
    bool peek_min(eq::Event& out) override;
    bool successor(eq::Time t, eq::Event& out) override;
    std::size_t size() const override { return count_; }
    const char* name() const override { return "avl"; }

    // ---- read-only inspection (checkers, experiments) ----
    const std::vector<AvlNode>& pool() const { return pool_; }
    const std::vector<std::uint32_t>& free_list() const { return free_; }
    std::uint32_t root() const { return root_; }
    int height() const { return pool_[root_].height; }
    // A "rebalance event" is one single or one double rotation (D5 counts these);
    // primitive rotations = single + 2 * double.
    std::uint64_t rebalance_single() const { return single_; }
    std::uint64_t rebalance_double() const { return double_; }
    std::size_t allocated_bytes() const {
        return pool_.capacity() * sizeof(AvlNode) + free_.capacity() * sizeof(std::uint32_t);
    }

protected:
    void do_insert(const eq::Event& e) override;

private:
    bool less(const eq::Key& x, const eq::Key& y) {
        EQ_COUNT(comparisons, 1);
        return x < y;
    }
    int h(std::uint32_t x) const { return pool_[x].height; }
    void update(std::uint32_t x);
    std::uint32_t rotate_right(std::uint32_t y);
    std::uint32_t rotate_left(std::uint32_t x);
    std::uint32_t rebalance(std::uint32_t x);
    std::uint32_t new_node(const eq::Event& e);
    void free_node(std::uint32_t x) { free_.push_back(x); }
    std::uint32_t insert_rec(std::uint32_t x, const eq::Event& e);
    std::uint32_t delete_min_rec(std::uint32_t x, eq::Event& out);
    std::uint32_t delete_rec(std::uint32_t x, const eq::Key& key, bool& found);

    std::vector<AvlNode> pool_;
    std::vector<std::uint32_t> free_;
    std::uint32_t root_ = 0;
    std::size_t count_ = 0;
    std::uint64_t single_ = 0;
    std::uint64_t double_ = 0;
};

}  // namespace structB
