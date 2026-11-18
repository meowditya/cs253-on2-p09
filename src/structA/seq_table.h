// src/structA/seq_table.h — open-addressing hash table, written from scratch, mapping
// seq -> (time, cancelled). Owner: Structure A owner.
//
// One record per event physically in the heap. The records flagged `cancelled` ARE the lazy
// cancellation set the handout asks for; time is stored so cancel() can reject forged handles.
//
// Design: linear probing, Fibonacci hashing, tombstones on erase, rebuild when
// occupied + tombstones > capacity / 2 (grow x2 if at least a quarter is genuinely occupied,
// otherwise rebuild at the same size to clear tombstones). Capacity is a power of two >= 16.
// The two largest uint64 values are reserved slot markers and are rejected as keys.
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace structA {

class SeqTable {
public:
    static constexpr std::uint64_t kEmpty = ~std::uint64_t{0};       // UINT64_MAX
    static constexpr std::uint64_t kTomb  = ~std::uint64_t{0} - 1;   // UINT64_MAX - 1

    struct Slot {
        std::uint64_t key;        // seq, or kEmpty / kTomb
        std::int64_t  time;
        std::uint8_t  cancelled;
    };

    // Counters may be null. When EQ_COUNTING is on, every slot inspected by find/insert_new adds one
    // to *probe_counter, and every slot inspected while re-inserting during a rebuild adds one to
    // *rehash_counter. Rebuild count and slots scanned/initialised are kept in all builds.
    explicit SeqTable(std::uint64_t* probe_counter = nullptr, std::uint64_t* rehash_counter = nullptr);

    // Home slot of key for a table of this capacity (Fibonacci hashing). Public so the invariant
    // checker can verify every record is reachable from its home before an EMPTY slot.
    std::size_t home(std::uint64_t key) const {
        return static_cast<std::size_t>((key * 0x9E3779B97F4A7C15ULL) >> shift_);
    }

    // Slot index holding key, or -1. Reserved keys are rejected before probing.
    long find(std::uint64_t key) const;

    // Adds a key that is NOT present (the heap only inserts fresh seqs). key < kTomb.
    void insert_new(std::uint64_t key, std::int64_t time);

    // Turns an occupied slot into a tombstone.
    void erase_at(std::size_t i);

    Slot& at(std::size_t i) { return slots_[i]; }
    const Slot& at(std::size_t i) const { return slots_[i]; }

    std::size_t used() const { return used_; }
    std::size_t tombs() const { return tombs_; }
    std::size_t capacity() const { return slots_.size(); }
    std::uint64_t rebuilds() const { return rebuilds_; }
    std::uint64_t rebuild_slot_work() const { return rebuild_work_; }   // old slots scanned + new slots initialised

    // Read-only walk over every occupied record (for the invariant checker).
    template <class F>
    void for_each_record(F f) const {
        for (const Slot& s : slots_)
            if (s.key != kEmpty && s.key != kTomb) f(s);
    }

private:
    void rebuild(std::size_t new_capacity);

    std::vector<Slot> slots_;
    std::size_t used_ = 0;
    std::size_t tombs_ = 0;
    unsigned shift_ = 60;            // 64 - log2(capacity)
    std::uint64_t rebuilds_ = 0;
    std::uint64_t rebuild_work_ = 0;
    std::uint64_t* probes_;
    std::uint64_t* rehash_probes_;
};

}  // namespace structA
