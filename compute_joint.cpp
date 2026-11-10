// Exact Guobiao best-14 shanten distribution for N = 14..max_n.
//
//   ./compute_joint [routes] [max_n] > distribution.txt
//
// routes: all (default; ordinary, seven pairs, thirteen orphans,
// all-not-connected, combination dragon), or names from base, anc, combo
// joined by '+', e.g. base+anc.
// Each output row is "N W(-1) W(0) ... W(13)" in physical-deal weights;
// divide by C(136, N) for probabilities. Every row is checked to sum to
// C(136, N). For routes containing base, N >= 38 is always a win.
#include "guobiao_engine.hpp"

#include <chrono>

int main(int argc, char** argv) {
    unsigned routes = gb::parse_routes(argc > 1 ? argv[1] : "all");
    int max_n = argc > 2 ? std::atoi(argv[2]) : 37;
    // C(136, N) < 2^128 only for N <= 54 (or N >= 82), and the deal weights are
    // unsigned __int128. Base routes already win for every deal from N = 38.
    if (max_n < 14 || max_n > 54) throw std::invalid_argument("max_n must be in 14..54 (unsigned __int128 weights)");
    auto start = std::chrono::steady_clock::now();
    gb::Engine engine(max_n);
    auto rows = engine.distribution(routes);
    std::cerr << engine.log();
    for (int n = 14; n <= max_n; ++n) {
        U128 sum = 0;
        for (auto w : rows[n]) sum += w;
        if (sum != choose_u128(136, n)) throw std::logic_error("total weight check failed at N=" + std::to_string(n));
        std::cout << n;
        for (auto w : rows[n]) std::cout << ' ' << u128_to_string(w);
        std::cout << '\n';
    }
    std::cerr << "elapsed " << std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() << " s\n";
}
