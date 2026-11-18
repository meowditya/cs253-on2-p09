// oracle/checkers.h — structural invariant checkers (INTERFACE.md §5). Owner: Oracle owner.
// Derivation D4 is about check_avl_view.
//
// Each checker works on a plain read-only "view" of a structure, so tests can also feed it
// deliberately corrupted states. Every checker returns "" when the state is valid, otherwise a
// message naming the first violation. Checkers never modify the structure or its counters.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "avl_queue.h"
#include "event_queue.h"
#include "lazy_heap.h"
#include "sorted_oracle.h"

namespace checkers {

// One hash-table slot, at its real position. `home` is the slot where probing for this key starts
// (meaningful only for occupied slots). Raw positions are kept so the checker can verify that every
// record is reachable by linear probing from its home before an EMPTY slot.
struct TableSlot {
    std::uint64_t key;        // seq, or SeqTable::kEmpty / kTomb
    std::int64_t  time;
    bool          cancelled;
    std::size_t   home;
};

struct HeapView {
    std::vector<eq::Event>   heap;          // the heap array, in array order
    std::vector<TableSlot>   slots;         // the whole table, slot by slot (capacity = slots.size())
    std::size_t table_used = 0, table_tombs = 0;   // the table's own counters
    std::size_t live = 0, cancelled = 0;    // the structure's L and C counters
    std::size_t reported_size = 0;          // size()
    eq::Seq issued = 0;
    bool purge_invariant = true;            // false for the no-purge experiment
};

struct AvlView {
    std::vector<structB::AvlNode> pool;
    std::vector<std::uint32_t> free_list;
    std::uint32_t root = 0;
    std::size_t reported_size = 0;
    eq::Seq issued = 0;
};

struct OracleView {
    std::vector<eq::Event> array;
    std::size_t reported_size = 0;
    eq::Seq issued = 0;
};

HeapView view_of(const structA::LazyHeap& q);
AvlView view_of(const structB::AvlQueue& q);
OracleView view_of(const oracle::SortedOracle& q);

std::string check_heap_view(const HeapView& v);
std::string check_avl_view(const AvlView& v);      // D4: accepts exactly valid AVL representations
std::string check_oracle_view(const OracleView& v);

// Dispatch on the dynamic type; "" for queue types without a checker.
std::string check_queue(const eq::EventQueue& q);

}  // namespace checkers
