// app/queue_factory.cpp — see queue_factory.h. Owner: Architect.
#include "queue_factory.h"

#include "avl_queue.h"
#include "lazy_heap.h"
#include "sorted_oracle.h"

namespace app {

std::unique_ptr<eq::EventQueue> make_queue(const std::string& name) {
    if (name == "heap") return std::make_unique<structA::LazyHeap>();
    if (name == "avl") return std::make_unique<structB::AvlQueue>();
    if (name == "oracle") return std::make_unique<oracle::SortedOracle>();
    if (name == "heap-nopurge") {
        structA::LazyHeapOptions o;
        o.purge = false;
        return std::make_unique<structA::LazyHeap>(o);
    }
    return nullptr;
}

}  // namespace app
