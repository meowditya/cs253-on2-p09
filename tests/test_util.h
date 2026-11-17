// tests/test_util.h — tiny test helpers shared by the unit tests (no framework needed).
#pragma once
#include <cstdio>
#include <cstdlib>
#include <string>

namespace testutil {

inline int& failures() { static int n = 0; return n; }

inline void check(bool ok, const std::string& what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what.c_str());
        ++failures();
    }
}

// Exit code 3 (= check failed) if anything failed, else print the summary and return 0.
inline int finish(const char* name) {
    if (failures()) {
        std::fprintf(stderr, "%s: %d check(s) failed\n", name, failures());
        return 3;
    }
    std::printf("%s: all checks passed\n", name);
    return 0;
}

}  // namespace testutil

#define CHECK(cond) ::testutil::check((cond), std::string(__FILE__) + ":" + std::to_string(__LINE__) + "  " #cond)
