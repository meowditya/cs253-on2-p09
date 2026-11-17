// tests/test_opstream.cpp — the .ops parser is strict. Owner: Architect.
#include <sstream>
#include <vector>

#include "opstream.h"
#include "test_util.h"

static bool parses(const std::string& text, std::vector<eq::Op>& ops) {
    std::istringstream in(text);
    std::string err;
    ops.clear();
    return eq::read_ops(in, "t.ops", ops, err);
}

int main() {
    std::vector<eq::Op> ops;
    CHECK(parses("# comment\n\nI 5 1 2\nC 0   # trailing comment\nF -3 18446744073709551615\nP\nK\nS -9223372036854775808\n", ops));
    CHECK(ops.size() == 6);
    CHECK(ops[0].kind == 'I' && ops[0].time == 5 && ops[0].type == 1 && ops[0].data == 2);
    CHECK(ops[2].kind == 'F' && ops[2].time == -3 && ops[2].seq == 18446744073709551615ULL);
    CHECK(ops[5].kind == 'S' && ops[5].time == std::numeric_limits<eq::Time>::min());
    // round trip
    std::ostringstream out;
    eq::write_ops(out, ops);
    std::vector<eq::Op> again;
    CHECK(parses(out.str(), again) && again.size() == ops.size() && eq::op_text(again[2]) == eq::op_text(ops[2]));
    // rejections
    for (const char* bad : {"X\n", "I 1 2\n", "I 1 2 3 4\n", "C -1\n", "C 1.5\n", "I 1 4294967296 0\n",
                            "S 9223372036854775808\n", "F 1\n", "P 3\n", "II 1 1 1\n", "C 0x10\n"}) {
        CHECK(!parses(bad, ops));
    }
    std::istringstream in("I 1 1 1\nI 2 2\n");
    std::string err;
    ops.clear();
    CHECK(!eq::read_ops(in, "f.ops", ops, err) && err.find("f.ops:2:") == 0);
    return testutil::finish("test_opstream");
}
