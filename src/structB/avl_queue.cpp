// src/structB/avl_queue.cpp — see avl_queue.h. Owner: Structure B owner.
#include "avl_queue.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace structB {

using eq::Event;
using eq::Key;

AvlQueue::AvlQueue() {
    pool_.push_back(AvlNode{Event{}, 0, 0, -1});   // index 0: NIL sentinel, never modified
}

void AvlQueue::update(std::uint32_t x) {
    pool_[x].height = static_cast<std::int8_t>(1 + std::max(h(pool_[x].left), h(pool_[x].right)));
}

// Right rotation at y (x = y.left):   y(x(A,B),C)  becomes  x(A,y(B,C)).
// In-order order A < x < B < y < C is unchanged; only heights of y and x need recomputing.
std::uint32_t AvlQueue::rotate_right(std::uint32_t y) {
    const std::uint32_t x = pool_[y].left;
    pool_[y].left = pool_[x].right;
    pool_[x].right = y;
    update(y);
    update(x);
    return x;
}

std::uint32_t AvlQueue::rotate_left(std::uint32_t x) {
    const std::uint32_t y = pool_[x].right;
    pool_[x].right = pool_[y].left;
    pool_[y].left = x;
    update(x);
    update(y);
    return y;
}

// Called on the way back up after every insert/delete step. Returns the subtree's new root.
std::uint32_t AvlQueue::rebalance(std::uint32_t x) {
    update(x);
    const int bf = h(pool_[x].left) - h(pool_[x].right);
    if (bf > 1) {                                            // left-heavy
        const std::uint32_t l = pool_[x].left;
        if (h(pool_[l].left) < h(pool_[l].right)) {          // LR: left child leans right
            pool_[x].left = rotate_left(l);
            ++double_;
            EQ_COUNT(rotations_double, 1);
        } else {                                             // LL (child balance 0 only after a delete)
            ++single_;
            EQ_COUNT(rotations_single, 1);
        }
        return rotate_right(x);
    }
    if (bf < -1) {                                           // right-heavy, mirror image
        const std::uint32_t r = pool_[x].right;
        if (h(pool_[r].right) < h(pool_[r].left)) {          // RL
            pool_[x].right = rotate_right(r);
            ++double_;
            EQ_COUNT(rotations_double, 1);
        } else {                                             // RR
            ++single_;
            EQ_COUNT(rotations_single, 1);
        }
        return rotate_left(x);
    }
    return x;
}

std::uint32_t AvlQueue::new_node(const Event& e) {
    if (!free_.empty()) {
        const std::uint32_t x = free_.back();
        free_.pop_back();
        pool_[x] = AvlNode{e, 0, 0, 0};
        return x;
    }
    if (pool_.size() >= std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("avl: node pool exhausted (2^32 - 1 nodes)");
    }
    pool_.push_back(AvlNode{e, 0, 0, 0});                    // may move the whole pool
    return static_cast<std::uint32_t>(pool_.size() - 1);
}

std::uint32_t AvlQueue::insert_rec(std::uint32_t x, const Event& e) {
    if (x == 0) return new_node(e);
    // Keys are unique, so "not less" means greater. The child index is copied out BEFORE the
    // recursive call (which may reallocate pool_), and pool_[x] is re-read afterwards.
    if (less(e.key, pool_[x].ev.key)) {
        const std::uint32_t child = insert_rec(pool_[x].left, e);
        pool_[x].left = child;
    } else {
        const std::uint32_t child = insert_rec(pool_[x].right, e);
        pool_[x].right = child;
    }
    return rebalance(x);
}

void AvlQueue::do_insert(const Event& e) {
    root_ = insert_rec(root_, e);
    ++count_;
}

// Removes the leftmost node of subtree x, copying its event into out.
std::uint32_t AvlQueue::delete_min_rec(std::uint32_t x, Event& out) {
    if (pool_[x].left == 0) {
        out = pool_[x].ev;
        const std::uint32_t r = pool_[x].right;
        free_node(x);
        return r;
    }
    const std::uint32_t child = delete_min_rec(pool_[x].left, out);
    pool_[x].left = child;
    return rebalance(x);
}

std::uint32_t AvlQueue::delete_rec(std::uint32_t x, const Key& key, bool& found) {
    if (x == 0) { found = false; return 0; }
    if (less(key, pool_[x].ev.key)) {
        const std::uint32_t child = delete_rec(pool_[x].left, key, found);
        pool_[x].left = child;
    } else if (less(pool_[x].ev.key, key)) {
        const std::uint32_t child = delete_rec(pool_[x].right, key, found);
        pool_[x].right = child;
    } else {
        found = true;
        if (pool_[x].left == 0) { const std::uint32_t r = pool_[x].right; free_node(x); return r; }
        if (pool_[x].right == 0) { const std::uint32_t l = pool_[x].left; free_node(x); return l; }
        Event succ{};                                        // two children: take the in-order
        const std::uint32_t child = delete_min_rec(pool_[x].right, succ);   // successor's whole
        pool_[x].right = child;                              // event into this node
        pool_[x].ev = succ;
    }
    if (!found) return x;                                    // key absent: nothing changed below
    return rebalance(x);
}

bool AvlQueue::cancel(const eq::Handle& h) {
    if (h.seq >= issued()) return false;
    bool found = false;
    root_ = delete_rec(root_, h, found);
    if (found) --count_;
    return found;
}

bool AvlQueue::pop_min(Event& out) {
    if (root_ == 0) return false;
    root_ = delete_min_rec(root_, out);
    --count_;
    return true;
}

bool AvlQueue::peek_min(Event& out) {
    if (root_ == 0) return false;
    std::uint32_t x = root_;
    while (pool_[x].left != 0) x = pool_[x].left;
    out = pool_[x].ev;
    return true;
}

// Keys with time > t are a suffix of the key order (time is compared first), so one walk finds
// the smallest of them: go left after recording a candidate, right otherwise.
bool AvlQueue::successor(eq::Time t, Event& out) {
    std::uint32_t x = root_, best = 0;
    while (x != 0) {
        if (pool_[x].ev.key.time > t) { best = x; x = pool_[x].left; }
        else x = pool_[x].right;
    }
    if (best == 0) return false;
    out = pool_[best].ev;
    return true;
}

}  // namespace structB
