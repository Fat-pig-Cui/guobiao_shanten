// Randomised comparison of the joint engine against direct per-deal
// evaluation, plus exact aggregate checks.
//
//   ./test_joint [deals_per_N]
#include "guobiao_engine.hpp"

#include <random>

using namespace std;

namespace {

const int kSkeleton[6][9] = {
    {0, 3, 6, 10, 13, 16, 20, 23, 26}, {0, 3, 6, 11, 14, 17, 19, 22, 25},
    {1, 4, 7, 11, 14, 17, 18, 21, 24}, {1, 4, 7, 9, 12, 15, 20, 23, 26},
    {2, 5, 8, 9, 12, 15, 19, 22, 25},  {2, 5, 8, 10, 13, 16, 18, 21, 24}};

// Independent reference: ordinary shape through the block solver, the
// other routes by explicit target enumeration over all 34 tile types.
struct Reference {
    Solver suit{9, true}, honour{7, false};
    vector<array<int, 34>> melds;
    Reference() {
        for (int s = 0; s < 3; ++s) for (int st = 0; st < 7; ++st) {
            array<int, 34> m{}; for (int k = 0; k < 3; ++k) m[9 * s + st + k] = 1; melds.push_back(m);
        }
        for (int i = 0; i < 34; ++i) { array<int, 34> m{}; m[i] = 3; melds.push_back(m); }
    }
    int base(const array<int, 34>& c) {
        uint64_t m = sig_bit(0, 0, 0);
        for (int b = 0; b < 4; ++b) {
            array<uint8_t, 9> x{};
            int len = b == 3 ? 7 : 9;
            for (int i = 0; i < len; ++i) x[i] = uint8_t(c[9 * b + i]);
            m = combine_mask(m, b == 3 ? honour.rec(x) : suit.rec(x));
        }
        int q = 0, u = 0; bool kp = false;
        for (int i = 0; i < 34; ++i) {
            q += c[i] / 2;
            bool term = i >= 27 || i % 9 == 0 || i % 9 == 8;
            u += term && c[i] > 0; kp |= term && c[i] >= 2;
        }
        return min({normal_shanten(m), 6 - min(q, 7), 13 - u - int(kp)});
    }
    int anc(const array<int, 34>& c) {
        int best = 0;
        int perm[3] = {0, 1, 2};
        do {
            int x = 0;
            for (int s = 0; s < 3; ++s) for (int k = 0; k < 3; ++k) x += c[9 * s + perm[s] + 3 * k] > 0;
            for (int i = 27; i < 34; ++i) x += c[i] > 0;
            best = max(best, min(x, 14));
        } while (next_permutation(perm, perm + 3));
        return 13 - best;
    }
    int combo(const array<int, 34>& c) {
        int best = 0;
        for (auto const& sk : kSkeleton) for (auto const& m : melds) for (int p = 0; p < 34; ++p) {
            array<int, 34> t = m;
            for (int x : sk) t[x]++;
            t[p] += 2;
            bool ok = true; int z = 0;
            for (int i = 0; i < 34; ++i) { ok &= t[i] <= 4; z += min(c[i], t[i]); }
            if (ok) best = max(best, z);
        }
        return 13 - best;
    }
    int value(const array<int, 34>& c, unsigned routes) {
        int v = 99;
        if (routes & gb::kBase) v = min(v, base(c));
        if (routes & gb::kAllNotConnected) v = min(v, anc(c));
        if (routes & gb::kComboDragon) v = min(v, combo(c));
        return v;
    }
};

int failures = 0;
void check(bool ok, const string& what) {
    cout << (ok ? "PASS " : "FAIL ") << what << "\n";
    failures += !ok;
}

}  // namespace

int main(int argc, char** argv) {
    int deals = argc > 1 ? atoi(argv[1]) : 2000;
    check(u128_to_string(choose_u128(136, 47)) == "8570510037891134237447359256263824000",
          "C(136,47) avoids intermediate overflow");
    check(u128_to_string(choose_u128(136, 54)) == "333443982108762453341292229611402248800",
          "C(136,54) fits in 128 bits");
    bool overflow = false;
    try { choose_u128(136, 55); } catch (const overflow_error&) { overflow = true; }
    check(overflow, "C(136,55) rejects overflow");
    const int max_n = 16;
    gb::Engine engine(max_n);
    Reference ref;

    mt19937_64 rng(20261109);
    vector<int> wall;
    for (int i = 0; i < 34; ++i) for (int j = 0; j < 4; ++j) wall.push_back(i);
    for (unsigned routes : {unsigned(gb::kAll), unsigned(gb::kBase), unsigned(gb::kAllNotConnected), unsigned(gb::kComboDragon)}) {
        int bad = 0, total = 0;
        for (int n : {14, 15, 17, 20, 24, 28, 33, 37}) for (int it = 0; it < deals; ++it) {
            shuffle(wall.begin(), wall.end(), rng);
            array<int, 34> c{};
            for (int i = 0; i < n; ++i) c[wall[i]]++;
            int got = gb::evaluate_deal(c, routes, engine.masks());
            int want = max(-1, ref.value(c, routes));
            ++total;
            if (got != want) {
                if (bad < 5) cerr << "routes=" << routes << " n=" << n << " engine " << got << " reference " << want << "\n";
                ++bad;
            }
        }
        check(bad == 0, "random deals routes=" + to_string(routes) + " (" + to_string(total) + " deals)");
    }

    auto all = engine.distribution(gb::kAll);
    for (int n = 14; n <= max_n; ++n) {
        U128 sum = 0;
        for (auto w : all[n]) sum += w;
        check(sum == choose_u128(136, n), "total weight N=" + to_string(n));
    }
    check(all[14][0] == U128(13257769189488ULL), "N=14 five-route winning weight = 13257769189488");

    // Base-only engine must agree with the independent block convolution.
    auto base = engine.distribution(gb::kBase);
    Rules rules = rules_by_name("yonma");
    rules.pairs = PairRule::Units;
    Map blocks = all_blocks(rules, max_n, true);
    bool same = true;
    for (int n = 14; n <= max_n; ++n) {
        array<U128, 15> want{};
        U128 alive = 0;
        for (auto const& [k, w] : blocks) if (k.k == n) { want[classify(rules, k) + 1] += w; alive += w; }
        want[0] = choose_u128(136, n) - alive;
        same &= want == base[n];
    }
    check(same, "base-only distribution matches shanten_core convolution");

    cout << engine.log();
    cout << (failures ? "FAILED\n" : "ALL PASSED\n");
    return failures != 0;
}
