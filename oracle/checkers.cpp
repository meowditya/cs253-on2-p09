// oracle/checkers.cpp — see checkers.h. Owner: Oracle owner.
#include "checkers.h"

#include <algorithm>
#include <cstdlib>

namespace checkers {

using eq::Event;
using eq::Key;

static std::string at(const char* what, std::size_t i) { return std::string(what) + " (index " + std::to_string(i) + ")"; }

// ------------------------------------------------------------------ views
HeapView view_of(const structA::LazyHeap& q) {
    HeapView v;
    v.heap = q.heap_array();
    const structA::SeqTable& t = q.table();
    v.slots.reserve(t.capacity());
    for (std::size_t i = 0; i < t.capacity(); ++i) {                 // reads slots directly: no
        const structA::SeqTable::Slot& s = t.at(i);                   // find(), so no counters move
        const bool occupied = s.key != structA::SeqTable::kEmpty && s.key != structA::SeqTable::kTomb;
        v.slots.push_back(TableSlot{s.key, s.time, s.cancelled != 0, occupied ? t.home(s.key) : 0});
    }
    v.table_used = t.used();
    v.table_tombs = t.tombs();
    v.live = q.live();
    v.cancelled = q.cancelled_in_heap();
    v.reported_size = q.size();
    v.issued = q.issued();
    v.purge_invariant = q.options().purge;   // only the explicit no-purge experiment is exempt
    return v;
}

AvlView view_of(const structB::AvlQueue& q) {
    return AvlView{q.pool(), q.free_list(), q.root(), q.size(), q.issued()};
}

OracleView view_of(const oracle::SortedOracle& q) {
    return OracleView{q.array(), q.size(), q.issued()};
}

// ------------------------------------------------------------------ Structure A
std::string check_heap_view(const HeapView& v) {
    const std::size_t m = v.heap.size();
    // 1. heap order
    for (std::size_t i = 1; i < m; ++i)
        if (v.heap[i].key < v.heap[(i - 1) / 2].key) return at("heap order violated", i);
    // 2. physical seqs unique and issued
    std::vector<std::pair<std::uint64_t, std::int64_t>> phys;
    phys.reserve(m);
    for (const Event& e : v.heap) {
        if (e.key.seq >= v.issued) return "heap holds a seq that was never issued";
        phys.emplace_back(e.key.seq, e.key.time);
    }
    std::sort(phys.begin(), phys.end());
    for (std::size_t i = 1; i < phys.size(); ++i)
        if (phys[i].first == phys[i - 1].first) return "duplicate seq in the heap array";
    // 3. table health and searchability: every record must be found by the table's own search,
    //    i.e. reachable by linear probing from its home slot before any EMPTY slot
    const std::size_t cap = v.slots.size();
    if (cap < 16 || (cap & (cap - 1)) != 0) return "table capacity is not a power of two >= 16";
    constexpr std::uint64_t kEmpty = structA::SeqTable::kEmpty, kTomb = structA::SeqTable::kTomb;
    std::vector<TableSlot> rec;
    std::size_t tombs = 0;
    for (std::size_t i = 0; i < cap; ++i) {
        const TableSlot& s = v.slots[i];
        if (s.key == kTomb) { ++tombs; continue; }
        if (s.key == kEmpty) continue;
        if (s.home >= cap) return at("table record has an out-of-range home slot", i);
        std::size_t j = s.home, steps = 0;
        while (j != i) {
            if (v.slots[j].key == kEmpty) return at("table record unreachable: EMPTY slot on its probe path", i);
            if (v.slots[j].key == s.key) return at("duplicate key in the hash table", i);
            j = (j + 1) & (cap - 1);
            if (++steps > cap) return at("table probe path does not terminate", i);
        }
        rec.push_back(s);
    }
    std::sort(rec.begin(), rec.end(), [](const TableSlot& a, const TableSlot& b) { return a.key < b.key; });
    for (std::size_t i = 1; i < rec.size(); ++i)
        if (rec[i].key == rec[i - 1].key) return "duplicate key in the hash table";
    if (v.table_used != rec.size()) return "table 'used' counter wrong";
    if (v.table_tombs != tombs) return "table 'tombs' counter wrong";
    if (2 * (v.table_used + v.table_tombs) > cap) return "table more than half full (occupied + tombstones)";
    // 4. the table mirrors the array exactly: same set of (seq, time), no orphans
    if (rec.size() != phys.size()) return "table records (" + std::to_string(rec.size()) + ") != heap entries (" + std::to_string(phys.size()) + ")";
    std::size_t live = 0, canc = 0;
    for (std::size_t i = 0; i < rec.size(); ++i) {
        if (rec[i].key != phys[i].first) return "table and heap hold different seqs (orphan record or missing record)";
        if (rec[i].time != phys[i].second) return "table time differs from heap time for a seq";
        (rec[i].cancelled ? canc : live) += 1;
    }
    // 5. counters
    if (live != v.live) return "live counter L does not match the table";
    if (canc != v.cancelled) return "cancelled counter C does not match the table";
    if (v.live != v.reported_size) return "size() differs from L";
    // 6. purge invariant
    if (v.purge_invariant && v.cancelled > v.live) return "purge invariant C <= L violated";
    return "";
}

// ------------------------------------------------------------------ Structure B (D4)
std::string check_avl_view(const AvlView& v) {
    const std::size_t n = v.pool.size();
    if (n == 0) return "pool has no NIL sentinel";
    if (v.pool[0].height != -1 || v.pool[0].left != 0 || v.pool[0].right != 0) return "NIL sentinel modified";
    if (v.root >= n) return "root index out of range";

    // Structural pre-pass (iterative): every edge in range, no node reached twice (no cycles,
    // no sharing). Collects nodes in preorder.
    std::vector<std::uint8_t> seen(n, 0);
    std::vector<std::uint32_t> order, stack;
    if (v.root != 0) stack.push_back(v.root);
    while (!stack.empty()) {
        const std::uint32_t x = stack.back();
        stack.pop_back();
        if (seen[x]) return at("node reached twice (cycle or shared child)", x);
        seen[x] = 1;
        order.push_back(x);
        for (std::uint32_t c : {v.pool[x].left, v.pool[x].right}) {
            if (c >= n) return at("child index out of range", x);
            if (c != 0) stack.push_back(c);
        }
    }

    // Post-order values: reverse preorder visits every child before its parent.
    std::vector<int> th(n, -1);
    std::vector<Key> mn(n), mx(n);
    for (auto it = order.rbegin(); it != order.rend(); ++it) {
        const std::uint32_t x = *it;
        const structB::AvlNode& node = v.pool[x];
        const Key k = node.ev.key;
        const int hl = node.left ? th[node.left] : -1;
        const int hr = node.right ? th[node.right] : -1;
        if (node.left && !(mx[node.left] < k)) return at("BST order violated (left subtree)", x);
        if (node.right && !(k < mn[node.right])) return at("BST order violated (right subtree)", x);
        const int t = 1 + std::max(hl, hr);
        if (node.height != t) return at("stored height wrong", x);          // recomputed, never trusted
        if (std::abs(hl - hr) > 1) return at("balance factor outside {-1,0,1}", x);
        th[x] = t;
        mn[x] = node.left ? mn[node.left] : k;
        mx[x] = node.right ? mx[node.right] : k;
    }

    // Representation: seqs, count, pool partition.
    std::vector<std::uint64_t> seqs;
    seqs.reserve(order.size());
    for (std::uint32_t x : order) {
        if (v.pool[x].ev.key.seq >= v.issued) return at("node holds a never-issued seq", x);
        seqs.push_back(v.pool[x].ev.key.seq);
    }
    std::sort(seqs.begin(), seqs.end());
    for (std::size_t i = 1; i < seqs.size(); ++i)
        if (seqs[i] == seqs[i - 1]) return "two nodes hold the same seq";
    if (order.size() != v.reported_size) return "reachable node count differs from size()";
    std::vector<std::uint8_t> freed(n, 0);
    for (std::uint32_t f : v.free_list) {
        if (f == 0 || f >= n) return "free list holds an invalid index";
        if (freed[f]) return "free list holds an index twice";
        if (seen[f]) return "a reachable node is also on the free list";
        freed[f] = 1;
    }
    if (order.size() + v.free_list.size() != n - 1) return "pool slots leaked: neither reachable nor free";
    return "";
}

// ------------------------------------------------------------------ oracle
std::string check_oracle_view(const OracleView& v) {
    for (std::size_t i = 0; i + 1 < v.array.size(); ++i)
        if (!(v.array[i + 1].key < v.array[i].key)) return at("oracle array not strictly descending", i);
    std::vector<std::uint64_t> seqs;
    for (const Event& e : v.array) {
        if (e.key.seq >= v.issued) return "oracle holds a never-issued seq";
        seqs.push_back(e.key.seq);
    }
    std::sort(seqs.begin(), seqs.end());
    for (std::size_t i = 1; i < seqs.size(); ++i)
        if (seqs[i] == seqs[i - 1]) return "oracle holds a seq twice";
    if (v.array.size() != v.reported_size) return "oracle size() mismatch";
    return "";
}

std::string check_queue(const eq::EventQueue& q) {
    if (auto* h = dynamic_cast<const structA::LazyHeap*>(&q)) return check_heap_view(view_of(*h));
    if (auto* a = dynamic_cast<const structB::AvlQueue*>(&q)) return check_avl_view(view_of(*a));
    if (auto* o = dynamic_cast<const oracle::SortedOracle*>(&q)) return check_oracle_view(view_of(*o));
    return "";
}

}  // namespace checkers
