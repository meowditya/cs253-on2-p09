// tests/test_oracle.cpp — hand-computed cases for the oracle, so the oracle is not only checked
// against itself. Owner: Oracle owner.
#include <limits>
#include <vector>

#include "checkers.h"
#include "sorted_oracle.h"
#include "test_util.h"

using eq::Event;
using eq::Handle;
using oracle::SortedOracle;

int main() {
    SortedOracle q;
    // insert times 5, 1, 9, 5, 3 -> keys (5,0) (1,1) (9,2) (5,3) (3,4)
    for (int t : {5, 1, 9, 5, 3}) q.insert(t, 0, static_cast<std::uint32_t>(t * 10));
    // descending array must be: (9,2) (5,3) (5,0) (3,4) (1,1)
    const std::vector<std::pair<long, unsigned long>> want = {{9, 2}, {5, 3}, {5, 0}, {3, 4}, {1, 1}};
    bool layout = q.array().size() == want.size();
    for (std::size_t i = 0; layout && i < want.size(); ++i)
        layout = q.array()[i].key.time == want[i].first && q.array()[i].key.seq == want[i].second;
    CHECK(layout);
    CHECK(checkers::check_oracle_view(checkers::view_of(q)).empty());

    Event e{};
    CHECK(q.successor(0, e) && e.key.seq == 1);      // smallest time > 0 is (1,1)
    CHECK(q.successor(1, e) && e.key.seq == 4);      // strictly greater: (3,4)
    CHECK(q.successor(3, e) && e.key.seq == 0);      // (5,0) before (5,3)
    CHECK(q.successor(5, e) && e.key.seq == 2);      // (9,2)
    CHECK(!q.successor(9, e));
    CHECK(q.successor(std::numeric_limits<eq::Time>::min(), e) && e.key.seq == 1);
    CHECK(!q.successor(std::numeric_limits<eq::Time>::max(), e));

    CHECK(!q.cancel(Handle{6, 3}));                  // wrong time
    CHECK(q.cancel(Handle{5, 3}) && !q.cancel(Handle{5, 3}));
    CHECK(!q.cancel(Handle{1, 5}));                  // seq 5 never issued
    CHECK(q.peek_min(e) && e.key.seq == 1 && q.size() == 4);
    CHECK(q.pop_min(e) && e.key.seq == 1 && e.data == 10);
    CHECK(q.pop_min(e) && e.key.seq == 4);
    CHECK(q.pop_min(e) && e.key.seq == 0);
    CHECK(q.pop_min(e) && e.key.seq == 2);
    CHECK(!q.pop_min(e) && q.empty());
    return testutil::finish("test_oracle");
}
