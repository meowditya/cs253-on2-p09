// src/structA/seq_table.cpp — see seq_table.h. Owner: Structure A owner.
#include "seq_table.h"

#include "event_queue.h"   // EQ_COUNTING

namespace structA {

SeqTable::SeqTable(std::uint64_t* probe_counter, std::uint64_t* rehash_counter)
    : slots_(16, Slot{kEmpty, 0, 0}), probes_(probe_counter), rehash_probes_(rehash_counter) {}

long SeqTable::find(std::uint64_t key) const {
    if (key >= kTomb) return -1;                       // reserved markers are never keys
    const std::size_t mask = slots_.size() - 1;
    std::size_t i = home(key);
    for (std::size_t n = 0; n < slots_.size(); ++n) {  // bounded: a full walk means a broken invariant
        if (EQ_COUNTING && probes_) ++*probes_;
        const std::uint64_t k = slots_[i].key;
        if (k == key) return static_cast<long>(i);
        if (k == kEmpty) return -1;                    // tombstones do not stop the search
        i = (i + 1) & mask;
    }
    return -1;
}

void SeqTable::insert_new(std::uint64_t key, std::int64_t time) {
    const std::size_t mask = slots_.size() - 1;
    std::size_t i = home(key);
    for (;;) {                                         // terminates: at least half the slots are free
        if (EQ_COUNTING && probes_) ++*probes_;
        const std::uint64_t k = slots_[i].key;
        if (k == kEmpty || k == kTomb) {
            if (k == kTomb) --tombs_;
            slots_[i] = Slot{key, time, 0};
            ++used_;
            break;
        }
        i = (i + 1) & mask;
    }
    if (2 * (used_ + tombs_) > slots_.size()) {
        rebuild(4 * used_ >= slots_.size() ? 2 * slots_.size() : slots_.size());
    }
}

void SeqTable::erase_at(std::size_t i) {
    slots_[i].key = kTomb;
    --used_;
    ++tombs_;
}

void SeqTable::rebuild(std::size_t new_capacity) {
    std::vector<Slot> old(new_capacity, Slot{kEmpty, 0, 0});
    old.swap(slots_);                                  // slots_ is now the new, empty array
    unsigned bits = 0;
    while ((std::size_t{1} << bits) < new_capacity) ++bits;
    shift_ = 64 - bits;
    const std::size_t mask = new_capacity - 1;
    ++rebuilds_;
    rebuild_work_ += old.size() + new_capacity;
    tombs_ = 0;
    for (const Slot& s : old) {
        if (s.key == kEmpty || s.key == kTomb) continue;
        std::size_t i = home(s.key);
        if (EQ_COUNTING && rehash_probes_) ++*rehash_probes_;
        while (slots_[i].key != kEmpty) {
            i = (i + 1) & mask;
            if (EQ_COUNTING && rehash_probes_) ++*rehash_probes_;
        }
        slots_[i] = s;                                 // keeps the cancelled flag
    }
}

}  // namespace structA
