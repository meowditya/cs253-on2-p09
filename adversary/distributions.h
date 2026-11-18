// adversary/distributions.h — the priority-increment distributions of Jones (1986), Table I,
// exactly as printed, used by the hold-model and cancellation-sweep benchmarks.
// Owner: Adversary. Header-only.
//
//   exponential  -ln(rand)                                    mean 1
//   uniform      2 rand                                       mean 1
//   biased       0.9 + 0.2 rand            (optional 5th)     mean 1
//   bimodal      0.95238 rand + (rand < 0.1 ? 9.5238 : 0)      mean 1.42857 (!)
//   triangular   1.5 rand^0.5                                 mean 1
//
// Jones's footnote says every distribution has mean 1, but the printed bimodal formula has mean
// 0.95238/2 + 0.1 * 9.5238 = 1.42857 (about 10/7). We keep the printed formula (two independent
// draws: first u, then v) and divide every sample by its ideal mean, so all five have mean 1
// before scaling to microseconds. Rounding to whole microseconds can create ties; the benchmark
// reports the fraction of zero increments, and a finer-scale run checks whether it matters.
#pragma once
#include <cmath>
#include <cstdint>
#include <string>

#include "rng.h"

namespace dist {

enum class Kind { Exponential, Uniform, Biased, Bimodal, Triangular };

inline bool parse(const std::string& s, Kind& k) {
    if (s == "exponential") k = Kind::Exponential;
    else if (s == "uniform") k = Kind::Uniform;
    else if (s == "biased") k = Kind::Biased;
    else if (s == "bimodal") k = Kind::Bimodal;
    else if (s == "triangular") k = Kind::Triangular;
    else return false;
    return true;
}

inline const char* name(Kind k) {
    switch (k) {
        case Kind::Exponential: return "exponential";
        case Kind::Uniform: return "uniform";
        case Kind::Biased: return "biased";
        case Kind::Bimodal: return "bimodal";
        case Kind::Triangular: return "triangular";
    }
    return "?";
}

// Ideal (continuous) mean of each printed formula.
inline double normalizer(Kind k) { return k == Kind::Bimodal ? 0.95238 * 0.5 + 0.1 * 9.5238 : 1.0; }

inline double sample(Kind k, eq::Rng& r) {
    switch (k) {
        case Kind::Exponential: return -std::log1p(-r.u01());      // u in [0,1): argument never 0
        case Kind::Uniform: return 2.0 * r.u01();
        case Kind::Biased: return 0.9 + 0.2 * r.u01();
        case Kind::Bimodal: {
            const double u = r.u01();
            const double v = r.u01();
            return 0.95238 * u + (v < 0.1 ? 9.5238 : 0.0);
        }
        case Kind::Triangular: return 1.5 * std::sqrt(r.u01());
    }
    return 0.0;
}

// Integer increment in microseconds with mean mean_us. False if the value is not finite or is
// outside [0, 2^62] (cannot happen for sane mean_us; checked anyway).
inline bool increment_us(Kind k, eq::Rng& r, std::int64_t mean_us, std::int64_t& out) {
    const double x = static_cast<double>(mean_us) * sample(k, r) / normalizer(k);
    if (!std::isfinite(x) || x < 0.0 || x > 4.6e18) return false;
    out = std::llround(x);
    return true;
}

}  // namespace dist
