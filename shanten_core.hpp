// Shared exact-enumeration core.
//
// A hand is a count vector over tile types. Each suit (or honour) block is
// enumerated locally: every local count vector is reduced to a summary
// (tile count, pair summary, kokushi summary, and the set of simultaneously
// feasible meld/taatsu/head signatures). Equal summaries are merged with their
// physical weight prod C(4, c_i), and blocks are convolved. Every global count
// vector has exactly one block split, so the convolution neither misses nor
// double counts physical deals.
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <future>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using U128 = unsigned __int128;

inline std::string u128_to_string(U128 x) {
    if (!x) return "0";
    std::string s;
    while (x) { s.push_back(char('0' + x % 10)); x /= 10; }
    std::reverse(s.begin(), s.end());
    return s;
}

inline U128 choose_u128(int n, int k) {
    if (k < 0 || k > n) return 0;
    k = std::min(k, n - k);
    U128 r = 1;
    for (int i = 1; i <= k; ++i) r = r * (n - k + i) / i;
    return r;
}

// A decomposition signature: complete melds m<=4, taatsu t<=4, head p<=1.
constexpr int sig_id(int m, int t, int p) { return (m * 5 + t) * 2 + p; }
constexpr uint64_t sig_bit(int m, int t, int p) { return uint64_t(1) << sig_id(m, t, p); }
constexpr uint64_t winning_mask =
    sig_bit(4, 0, 1) | sig_bit(4, 1, 1) | sig_bit(4, 2, 1) | sig_bit(4, 3, 1) | sig_bit(4, 4, 1);

inline int normal_shanten(uint64_t mask) {
    int best = 99;
    for (int m = 0; m <= 4; ++m) for (int t = 0; t <= 4; ++t) for (int p = 0; p <= 1; ++p)
        if (mask & sig_bit(m, t, p)) best = std::min(best, 8 - 2 * m - std::min(t, 4 - m) - p);
    return best;
}

// Signatures of two disjoint tile sets add; keep only those within the caps.
inline uint64_t combine_mask(uint64_t a, uint64_t b) {
    uint64_t out = 0;
    for (int m1 = 0; m1 <= 4; ++m1) for (int t1 = 0; t1 <= 4; ++t1) for (int p1 = 0; p1 <= 1; ++p1)
        if (a & sig_bit(m1, t1, p1))
            for (int m2 = 0; m2 <= 4; ++m2) for (int t2 = 0; t2 <= 4; ++t2) for (int p2 = 0; p2 <= 1; ++p2)
                if ((b & sig_bit(m2, t2, p2)) && m1 + m2 <= 4 && t1 + t2 <= 4 && p1 + p2 <= 1)
                    out |= sig_bit(m1 + m2, t1 + t2, p1 + p2);
    return out;
}

// Memoised set of all feasible (m,t,p) signatures of one local block.
struct Solver {
    int len; bool sequences; std::vector<int> pow5; std::vector<uint64_t> memo;
    Solver(int n, bool s) : len(n), sequences(s), pow5(n + 1, 1) {
        for (int i = 1; i <= n; ++i) pow5[i] = pow5[i - 1] * 5;
        memo.assign(pow5[n], 0);
    }
    int encode(const std::array<uint8_t, 9>& c) const {
        int x = 0; for (int i = 0; i < len; ++i) x += c[i] * pow5[i]; return x;
    }
    uint64_t rec(const std::array<uint8_t, 9>& c) {
        int code = encode(c); if (memo[code]) return memo[code];
        int i = 0; while (i < len && c[i] == 0) ++i;
        if (i == len) return memo[code] = sig_bit(0, 0, 0);
        std::array<uint8_t, 9> d = c; uint64_t out = 0;
        auto merge = [&](uint64_t child, int dm, int dt, int dp) {
            for (int m = 0; m <= 4; ++m) for (int t = 0; t <= 4; ++t) for (int p = 0; p <= 1; ++p)
                if (child & sig_bit(m, t, p)) {
                    int mm = m + dm, tt = t + dt, pp = p + dp;
                    if (mm <= 4 && tt <= 4 && pp <= 1) out |= sig_bit(mm, tt, pp);
                }
        };
        // Isolated tile, pair as taatsu or head, triplet, sequence, adjacent and gapped taatsu.
        d[i]--; out |= rec(d);
        if (c[i] >= 2) { d = c; d[i] -= 2; auto ch = rec(d); merge(ch, 0, 1, 0); merge(ch, 0, 0, 1); }
        if (c[i] >= 3) { d = c; d[i] -= 3; merge(rec(d), 1, 0, 0); }
        if (sequences && i < len - 2 && i % 9 <= 6 && c[i + 1] && c[i + 2]) { d = c; d[i]--; d[i + 1]--; d[i + 2]--; merge(rec(d), 1, 0, 0); }
        if (sequences && i < len - 1 && i % 9 <= 7 && c[i + 1]) { d = c; d[i]--; d[i + 1]--; merge(rec(d), 0, 1, 0); }
        if (sequences && i < len - 2 && i % 9 <= 6 && c[i + 2]) { d = c; d[i]--; d[i + 2]--; merge(rec(d), 0, 1, 0); }
        return memo[code] = out;
    }
};

// Distinct: seven pairs need seven different types (riichi).
// Units:    a type with c tiles gives floor(c/2) pairs (dazhong / Chiyu).
enum class PairRule { Distinct, Units };

struct Block { int len; bool sequences; std::vector<uint8_t> terminal; };

struct Rules {
    std::string name; int tiles; int types; PairRule pairs; bool kokushi;
    std::vector<Block> blocks;
};

inline Block suit_block() { return {9, true, {1, 0, 0, 0, 0, 0, 0, 0, 1}}; }
inline Block honour_block() { return {7, false, {1, 1, 1, 1, 1, 1, 1}}; }

// yonma: 136 tiles. sanma: 1m/9m only, 108 tiles. dazhong: three suits only,
// repeated pairs allowed and no kokushi, 108 tiles.
inline Rules rules_by_name(const std::string& name) {
    if (name == "yonma") return {name, 136, 34, PairRule::Distinct, true, {suit_block(), suit_block(), suit_block(), honour_block()}};
    if (name == "sanma") return {name, 108, 27, PairRule::Distinct, true, {{2, false, {1, 1}}, suit_block(), suit_block(), honour_block()}};
    if (name == "dazhong") return {name, 108, 27, PairRule::Units, false, {suit_block(), suit_block(), suit_block()}};
    throw std::invalid_argument("unknown rules: " + name);
}

// Seven-pairs shanten from the pair summary q and distinct-type count d.
inline int chiitoi_shanten(const Rules& rules, int q, int d) {
    return rules.pairs == PairRule::Distinct ? 6 - q + std::max(0, 7 - d) : 6 - q;
}

// q and d are capped at seven: a seven-pairs structure then uses at most 14
// tiles, so the summary also describes the best 14-tile subset of a larger
// observation.
struct Key {
    uint8_t k, q, d, u, kp; uint64_t mask;
    bool operator==(Key const& o) const { return k == o.k && q == o.q && d == o.d && u == o.u && kp == o.kp && mask == o.mask; }
};
struct KeyHash {
    size_t operator()(Key const& x) const {
        uint64_t h = x.mask + 0x9e3779b97f4a7c15ULL;
        h ^= x.k + 0x9e37 + (h << 6) + (h >> 2); h ^= x.q + 0x85eb + (h << 6) + (h >> 2);
        h ^= x.d + 0xc2b2 + (h << 6) + (h >> 2); h ^= x.u + 0x27d4 + (h << 6) + (h >> 2);
        h ^= x.kp + (h << 6) + (h >> 2);
        return h;
    }
};
using Map = std::unordered_map<Key, U128, KeyHash>;

// The observation already contains a winning 14-tile subset.
inline bool contains_win(const Rules& rules, const Key& x) {
    return x.q >= 7 || (rules.kokushi && x.u == 13 && x.kp) || (x.mask & winning_mask);
}

// Shanten of the best 14-tile subset (or of the hand itself for 13/14 tiles).
inline int classify(const Rules& rules, const Key& x) {
    int value = std::min(normal_shanten(x.mask), chiitoi_shanten(rules, x.q, x.d));
    if (rules.kokushi) value = std::min(value, 13 - x.u - (x.kp ? 1 : 0));
    return value;
}

// All local summaries of one block with at most max_tiles tiles. With
// drop_won, summaries that already contain a win are discarded (absorbing).
inline Map block_summaries(const Rules& rules, const Block& block, int max_tiles, bool drop_won) {
    Solver solver(block.len, block.sequences);
    Map out;
    std::array<uint8_t, 9> c{};
    for (int code = 0; code < solver.pow5[block.len]; ++code) {
        int z = code, k = 0, q = 0, d = 0, u = 0; bool kp = false; int weight = 1;
        for (int i = 0; i < block.len; ++i) {
            c[i] = z % 5; z /= 5; k += c[i];
            q += rules.pairs == PairRule::Distinct ? (c[i] >= 2) : c[i] / 2;
            d += c[i] > 0; u += block.terminal[i] && c[i] > 0; kp |= block.terminal[i] && c[i] >= 2;
            weight *= int(choose_u128(4, c[i]));
        }
        if (k > max_tiles) continue;
        Key key{(uint8_t)k, (uint8_t)std::min(q, 7), (uint8_t)std::min(d, 7), (uint8_t)u, (uint8_t)kp, solver.rec(c)};
        if (drop_won && contains_win(rules, key)) continue;
        out[key] += weight;
    }
    return out;
}

inline Map convolve(const Rules& rules, const Map& a, const Map& b, int max_tiles, bool drop_won) {
    Map out;
    for (auto const& [x, v] : a) for (auto const& [y, w] : b) {
        int k = x.k + y.k; if (k > max_tiles) continue;
        int q = std::min(7, x.q + y.q), u = x.u + y.u; bool kp = x.kp || y.kp;
        if (drop_won && (q >= 7 || (rules.kokushi && u == 13 && kp))) continue;
        Key z{(uint8_t)k, (uint8_t)q, (uint8_t)std::min(7, x.d + y.d), (uint8_t)u, (uint8_t)kp, combine_mask(x.mask, y.mask)};
        if (drop_won && contains_win(rules, z)) continue;
        out[z] += v * w;
    }
    return out;
}

// Convolution of every block of the rules, up to max_tiles observed tiles.
inline Map all_blocks(const Rules& rules, int max_tiles, bool drop_won) {
    Map cur;
    const Block* previous = nullptr; Map previous_map;
    for (auto const& block : rules.blocks) {
        bool same = previous && previous->len == block.len && previous->sequences == block.sequences && previous->terminal == block.terminal;
        if (!same) { previous_map = block_summaries(rules, block, max_tiles, drop_won); previous = &block; }
        cur = cur.empty() ? previous_map : convolve(rules, cur, previous_map, max_tiles, drop_won);
    }
    return cur;
}
