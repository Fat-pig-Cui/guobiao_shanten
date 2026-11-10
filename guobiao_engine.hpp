// Exact joint shanten distribution for Guobiao best-14 selection.
//
// For a threshold t the engine counts the physical deals C whose best-14
// shanten is still greater than t on every enabled route ("alive" deals).
// The distribution follows from P(S = r) = alive(r - 1) - alive(r).
//
// Every route value only improves when tiles are added, so a partial deal
// that already reaches t on any route can be dropped (absorbing). Values
// that can no longer reach the threshold are clamped to zero, which keeps
// the state space small. All decisions are taken on one observed deal, so
// the result is the true per-deal minimum over routes.
//
// Block order: suit, suit (unordered pair, reflection-canonical), suit,
// honours. Per-suit summaries do not depend on which suit they belong to:
// all-not-connected and combination-dragon values are indexed by the
// position group g (tiles g, g+3, g+6) instead of by an absolute target.
#pragma once

#include "shanten_core.hpp"

#include <thread>

namespace gb {

enum Route : unsigned { kBase = 1, kAllNotConnected = 2, kComboDragon = 4, kAll = 7 };

// "all", or route names joined by '+', e.g. "base+anc".
inline unsigned parse_routes(const std::string& spec) {
    if (spec == "all") return kAll;
    unsigned routes = 0;
    std::stringstream in(spec);
    for (std::string name; std::getline(in, name, '+');) {
        if (name == "base") routes |= kBase;
        else if (name == "anc") routes |= kAllNotConnected;
        else if (name == "combo") routes |= kComboDragon;
        else throw std::invalid_argument("unknown route: " + name);
    }
    if (!routes) throw std::invalid_argument("empty route set");
    return routes;
}

// Suit and pair states use a[g] and d[4g+e]. The three-suit state keeps the
// assignment maximum in a[0] and d[0..3]. Honour states keep the number of
// distinct honours in a[0] and the combination-dragon meld/pair values in
// d[0..3]. Bit 1 of e means the meld is placed in this block, bit 0 the pair.
struct State {
    uint8_t k = 0, q = 0, u = 0, kp = 0;
    uint16_t mask = 0;
    std::array<uint8_t, 3> a{};
    std::array<uint8_t, 12> d{};
};

struct Packed {
    uint64_t lo, hi;
    bool operator==(const Packed& o) const { return lo == o.lo && hi == o.hi; }
    bool operator<(const Packed& o) const { return hi != o.hi ? hi < o.hi : lo < o.lo; }
};
struct PackedHash {
    size_t operator()(const Packed& p) const {
        uint64_t h = p.lo * 0x9e3779b97f4a7c15ULL ^ (p.hi + 0x632be59bd9b4e019ULL) * 0xbf58476d1ce4e5b9ULL;
        return h ^ (h >> 31);
    }
};

inline Packed pack(const State& s) {
    Packed p{uint64_t(s.k) | uint64_t(s.q) << 6 | uint64_t(s.u) << 10 | uint64_t(s.kp) << 14 | uint64_t(s.mask) << 15, 0};
    for (int g = 0; g < 3; ++g) p.lo |= uint64_t(s.a[g]) << (31 + 4 * g);
    for (int i = 0; i < 12; ++i) p.hi |= uint64_t(s.d[i]) << (4 * i);
    return p;
}

inline State unpack(const Packed& p) {
    State s;
    s.k = p.lo & 63; s.q = (p.lo >> 6) & 15; s.u = (p.lo >> 10) & 15; s.kp = (p.lo >> 14) & 1;
    s.mask = (p.lo >> 15) & 0xffff;
    for (int g = 0; g < 3; ++g) s.a[g] = (p.lo >> (31 + 4 * g)) & 15;
    for (int i = 0; i < 12; ++i) s.d[i] = (p.hi >> (4 * i)) & 15;
    return s;
}

using Table = std::vector<std::pair<State, U128>>;
using StateMap = std::unordered_map<Packed, U128, PackedHash>;

inline Table to_table(const StateMap& m) {
    Table t;
    t.reserve(m.size());
    for (auto const& [k, w] : m) t.push_back({unpack(k), w});
    std::sort(t.begin(), t.end(), [](auto const& x, auto const& y) { return pack(x.first) < pack(y.first); });
    return t;
}

// Registry of ordinary-shape signature masks. Masks keep only signatures
// with t <= 4 - m; masks are downward closed, so this loses nothing.
// Combination results are precomputed into a dense table before parallel
// stages; right operands are always single-block masks ("base" ids).
class Masks {
public:
    int id(uint64_t raw) {
        uint64_t m = 0;
        for (int a = 0; a <= 4; ++a) for (int t = 0; t <= 4 - a; ++t) for (int p = 0; p <= 1; ++p)
            if (raw & sig_bit(a, t, p)) m |= sig_bit(a, t, p);
        auto it = ids_.find(m);
        if (it != ids_.end()) return it->second;
        int i = int(masks_.size());
        if (i > 0xffff) throw std::runtime_error("too many masks");
        masks_.push_back(m); ids_[m] = i; shanten_.push_back(int8_t(normal_shanten(m)));
        return i;
    }
    void freeze_base() { base_ = int(masks_.size()); }
    int shanten(int i) const { return shanten_[i]; }
    // Fill combine(l, r) for every l in left and r in right.
    void prepare(const std::vector<int>& left, const std::vector<int>& right) {
        for (int l : left) for (int r : right) {
            if (r >= base_) throw std::logic_error("right operand must be a block mask");
            size_t at = size_t(l) * base_ + r;
            if (at < table_.size() && table_[at] >= 0) continue;
            int v = id(combine_mask(masks_[l], masks_[r]));
            size_t need = masks_.size() * size_t(base_);
            if (table_.size() < need) table_.resize(need, -1);
            table_[at] = v;
        }
        size_t need = masks_.size() * size_t(base_);
        if (table_.size() < need) table_.resize(need, -1);
    }
    int combine(int l, int r) const { return table_[size_t(l) * base_ + r]; }
    int zero() const { return 0; }  // id of the mask {(0,0,0)}, registered first

private:
    std::vector<uint64_t> masks_;
    std::unordered_map<uint64_t, int> ids_;
    std::vector<int8_t> shanten_;
    std::vector<int> table_;
    int base_ = 0;
};

// Combination-dragon additions inside one suit for skeleton group g:
// targets[g][e] lists the 9-tile target vectors (skeleton plus meld/pair).
struct ComboTargets {
    std::array<std::array<std::vector<std::array<uint8_t, 9>>, 4>, 3> targets;
    ComboTargets() {
        for (int g = 0; g < 3; ++g) {
            std::array<uint8_t, 9> sk{};
            sk[g] = sk[g + 3] = sk[g + 6] = 1;
            std::vector<std::array<uint8_t, 9>> melds;
            for (int s = 0; s < 7; ++s) { auto m = sk; m[s]++; m[s + 1]++; m[s + 2]++; melds.push_back(m); }
            for (int x = 0; x < 9; ++x) { auto m = sk; m[x] += 3; melds.push_back(m); }
            auto valid = [](const std::array<uint8_t, 9>& v) { for (int x : v) if (x > 4) return false; return true; };
            targets[g][0].push_back(sk);
            for (int p = 0; p < 9; ++p) { auto v = sk; v[p] += 2; if (valid(v)) targets[g][1].push_back(v); }
            for (auto const& m : melds) {
                if (valid(m)) targets[g][2].push_back(m);
                for (int p = 0; p < 9; ++p) { auto v = m; v[p] += 2; if (valid(v)) targets[g][3].push_back(v); }
            }
        }
    }
};

inline State suit_summary(const std::array<uint8_t, 9>& c, Solver& solver, Masks& masks) {
    static const ComboTargets combo;
    State s;
    int q = 0;
    for (int i = 0; i < 9; ++i) { s.k += c[i]; q += c[i] / 2; }
    s.q = uint8_t(std::min(q, 7));
    s.u = (c[0] > 0) + (c[8] > 0);
    s.kp = c[0] >= 2 || c[8] >= 2;
    s.mask = uint16_t(masks.id(solver.rec(c)));
    for (int g = 0; g < 3; ++g) {
        s.a[g] = (c[g] > 0) + (c[g + 3] > 0) + (c[g + 6] > 0);
        for (int e = 0; e < 4; ++e) {
            int best = 0;
            for (auto const& t : combo.targets[g][e]) {
                int z = 0;
                for (int i = 0; i < 9; ++i) z += std::min(c[i], t[i]);
                best = std::max(best, z);
            }
            s.d[4 * g + e] = uint8_t(best);
        }
    }
    return s;
}

inline State honour_summary(const std::array<uint8_t, 9>& c, Solver& solver, Masks& masks) {
    State s;
    int q = 0, distinct = 0, pair = 0, meld = 0, both = 0;
    for (int i = 0; i < 7; ++i) {
        s.k += c[i]; q += c[i] / 2; distinct += c[i] > 0;
        s.kp |= c[i] >= 2;
        pair = std::max(pair, std::min<int>(c[i], 2));
        meld = std::max(meld, std::min<int>(c[i], 3));
        for (int j = 0; j < 7; ++j)
            if (j != i) both = std::max(both, std::min<int>(c[i], 3) + std::min<int>(c[j], 2));
    }
    s.q = uint8_t(std::min(q, 7));
    s.u = uint8_t(distinct);
    s.mask = uint16_t(masks.id(solver.rec(c)));
    s.a[0] = uint8_t(distinct);
    s.d[0] = 0; s.d[1] = uint8_t(pair); s.d[2] = uint8_t(meld); s.d[3] = uint8_t(both);
    return s;
}

// One threshold pass: "alive" means every enabled route is still > t.
struct Pass {
    int t; unsigned routes;
    int T() const { return 13 - t; }  // special-route overlap that reaches t
    bool base() const { return routes & kBase; }
    bool anc() const { return routes & kAllNotConnected; }
    bool combo() const { return routes & kComboDragon; }
};

inline void restrict_routes(State& s, const Pass& p, const Masks& masks) {
    if (!p.base()) { s.q = s.u = s.kp = 0; s.mask = uint16_t(masks.zero()); }
    if (!p.anc()) s.a.fill(0);
    if (!p.combo()) s.d.fill(0);
}

// Values are lower bounds of the final route values, so reaching the
// threshold early is final.
inline bool dead(const State& s, const Pass& p, const Masks& masks) {
    if (p.base() && (masks.shanten(s.mask) <= p.t || 6 - s.q <= p.t || 13 - s.u - s.kp <= p.t)) return true;
    if (p.anc()) for (int x : s.a) if (x >= p.T()) return true;
    if (p.combo()) for (int x : s.d) if (x >= p.T()) return true;
    return false;
}

inline int future_combo(int e) { return 3 * !(e & 2) + 2 * !(e & 1); }

// Two suits: a[g] / d[4g+e] are the best values using the two groups other
// than g, assigned to the two suits in either order.
inline State combine_pair(const State& x, const State& y, const Pass& p, const Masks& masks) {
    State z;
    z.k = x.k + y.k;
    if (p.base()) {
        z.q = uint8_t(std::min(7, x.q + y.q)); z.u = x.u + y.u; z.kp = x.kp | y.kp;
        z.mask = uint16_t(masks.combine(x.mask, y.mask));
    } else {
        z.mask = uint16_t(masks.zero());
    }
    for (int g = 0; g < 3; ++g) {
        int ga = (g + 1) % 3, gb = (g + 2) % 3;
        if (p.anc()) z.a[g] = uint8_t(std::max(x.a[ga] + y.a[gb], x.a[gb] + y.a[ga]));
        if (p.combo())
            for (int e = 0; e < 4; ++e) {
                int best = 0;
                for (int e1 = 0; e1 < 4; ++e1) if ((e1 & e) == e1) {
                    int e2 = e ^ e1;
                    best = std::max({best, x.d[4 * ga + e1] + y.d[4 * gb + e2], x.d[4 * gb + e1] + y.d[4 * ga + e2]});
                }
                z.d[4 * g + e] = uint8_t(best);
            }
    }
    return z;
}

// Remaining blocks after two suits: one suit (<= 3 group tiles, <= 3 + meld
// + pair combo tiles, <= 2 terminals) and honours (<= 7 distinct).
inline void clamp_pair(State& s, const Pass& p) {
    if (p.anc()) for (auto& x : s.a) if (x + 10 < p.T()) x = 0;
    if (p.combo()) for (int i = 0; i < 12; ++i) if (s.d[i] + 3 + future_combo(i & 3) < p.T()) s.d[i] = 0;
    if (p.base() && s.u + 10 < 13 - p.t) s.u = s.kp = 0;
}

// Reflection i -> 8 - i of every suit maps group 0 <-> 2 and preserves all
// other summaries and weights. Pair states may be merged with their mirror
// image because the third-suit table is mirror symmetric.
inline Packed canonical_pair(const State& s) {
    State r = s;
    std::swap(r.a[0], r.a[2]);
    for (int e = 0; e < 4; ++e) std::swap(r.d[e], r.d[8 + e]);
    Packed a = pack(s), b = pack(r);
    return b < a ? b : a;
}

inline State combine_triple(const State& x, const State& y, const Pass& p, const Masks& masks) {
    State z;
    z.k = x.k + y.k;
    if (p.base()) {
        z.q = uint8_t(std::min(7, x.q + y.q)); z.u = x.u + y.u; z.kp = x.kp | y.kp;
        z.mask = uint16_t(masks.combine(x.mask, y.mask));
    } else {
        z.mask = uint16_t(masks.zero());
    }
    for (int g = 0; g < 3; ++g) {
        if (p.anc()) z.a[0] = uint8_t(std::max<int>(z.a[0], x.a[g] + y.a[g]));
        if (p.combo())
            for (int e = 0; e < 4; ++e)
                for (int e1 = 0; e1 < 4; ++e1) if ((e1 & e) == e1)
                    z.d[e] = uint8_t(std::max<int>(z.d[e], x.d[4 * g + e1] + y.d[4 * g + (e ^ e1)]));
    }
    return z;
}

inline void clamp_triple(State& s, const Pass& p) {
    if (p.anc() && s.a[0] + 7 < p.T()) s.a[0] = 0;
    if (p.combo()) for (int e = 0; e < 4; ++e) if (s.d[e] + future_combo(e) < p.T()) s.d[e] = 0;
    if (p.base() && s.u + 8 < 13 - p.t) s.u = s.kp = 0;
}

// Final decision for three suits x and honours h. The all-not-connected
// value is min(14, groups + honours) and 13 - t <= 14, so the cap is moot.
inline bool final_alive(const State& x, const State& h, const Pass& p, const Masks& masks) {
    if (p.base()) {
        if (masks.shanten(masks.combine(x.mask, h.mask)) <= p.t) return false;
        if (6 - std::min(7, x.q + h.q) <= p.t) return false;
        if (13 - (x.u + h.u) - (x.kp | h.kp) <= p.t) return false;
    }
    if (p.anc() && x.a[0] + h.a[0] >= p.T()) return false;
    if (p.combo())
        for (int e = 0; e < 4; ++e) if (x.d[e] + h.d[3 ^ e] >= p.T()) return false;
    return true;
}

class Engine {
public:
    explicit Engine(int max_n, unsigned threads = std::max(1u, std::thread::hardware_concurrency()))
        : max_n_(max_n), threads_(threads) {
        masks_.id(sig_bit(0, 0, 0));
        Solver suit_solver(9, true), honour_solver(7, false);
        StateMap suits, honours;
        std::array<uint8_t, 9> c{};
        for (int code = 0; code < 1953125; ++code) {
            int z = code, k = 0; U128 w = 1;
            for (int i = 0; i < 9; ++i) { c[i] = z % 5; z /= 5; k += c[i]; w *= choose_u128(4, c[i]); }
            // Register every block mask so that single-deal evaluation of
            // larger deals still finds its masks in the base set.
            State s = suit_summary(c, suit_solver, masks_);
            if (k <= max_n_) suits[pack(s)] += w;
        }
        c.fill(0);
        for (int code = 0; code < 78125; ++code) {
            int z = code, k = 0; U128 w = 1;
            for (int i = 0; i < 7; ++i) { c[i] = z % 5; z /= 5; k += c[i]; w *= choose_u128(4, c[i]); }
            State s = honour_summary(c, honour_solver, masks_);
            if (k <= max_n_) honours[pack(s)] += w;
        }
        masks_.freeze_base();
        suits_ = to_table(suits);
        honours_ = to_table(honours);
    }

    Masks& masks() { return masks_; }

    // alive[n] for n = 0..max_n: weight of n-tile deals with every enabled
    // route still above t.
    std::vector<U128> alive(const Pass& p) {
        Table L = restrict_table(suits_, p), H = restrict_table(honours_, p);
        Table P = pair_stage(L, p);
        Table R = triple_stage(P, L, p);
        log_ << "t=" << p.t << " suit " << L.size() << " pair " << P.size() << " triple " << R.size()
             << " honour " << H.size() << "\n";
        return final_stage(R, H, p);
    }

    // Weights W[n][r + 1] for r = -1..13.
    std::vector<std::array<U128, 15>> distribution(unsigned routes) {
        std::vector<std::vector<U128>> alive_by_t;
        for (int t = -1; t <= 12; ++t) {
            alive_by_t.push_back(alive(Pass{t, routes}));
            bool any = false;
            for (int n = 14; n <= max_n_; ++n) any |= alive_by_t.back()[n] != 0;
            if (!any) break;
        }
        std::vector<std::array<U128, 15>> out(max_n_ + 1);
        for (int n = 14; n <= max_n_; ++n) {
            U128 prev = choose_u128(136, n);
            for (int r = -1; r <= 13; ++r) {
                int i = r + 1;
                U128 cur = i < int(alive_by_t.size()) ? alive_by_t[i][n] : 0;
                if (cur > prev) throw std::logic_error("alive weights are not monotone");
                out[n][r + 1] = prev - cur;
                prev = cur;
            }
        }
        return out;
    }

    const std::string log() const { return log_.str(); }

private:
    Table restrict_table(const Table& raw, const Pass& p) {
        StateMap m;
        for (auto const& [s0, w] : raw) {
            State s = s0;
            restrict_routes(s, p, masks_);
            if (!dead(s, p, masks_)) m[pack(s)] += w;
        }
        return to_table(m);
    }

    static std::vector<int> mask_ids(const Table& t) {
        std::vector<int> ids;
        for (auto const& [s, w] : t) ids.push_back(s.mask);
        std::sort(ids.begin(), ids.end());
        ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
        return ids;
    }

    template <class F>
    Table parallel_merge(size_t n, F&& work) {
        std::vector<StateMap> parts(threads_);
        std::vector<std::thread> pool;
        for (unsigned i = 0; i < threads_; ++i)
            pool.emplace_back([&, i] { for (size_t j = i; j < n; j += threads_) work(j, parts[i]); });
        for (auto& th : pool) th.join();
        StateMap all = std::move(parts[0]);
        for (unsigned i = 1; i < threads_; ++i) for (auto const& [k, w] : parts[i]) all[k] += w;
        return to_table(all);
    }

    Table pair_stage(const Table& L, const Pass& p) {
        if (p.base()) { auto ids = mask_ids(L); masks_.prepare(ids, ids); }
        return parallel_merge(L.size(), [&](size_t i, StateMap& out) {
            for (size_t j = i; j < L.size(); ++j) {
                if (L[i].first.k + L[j].first.k > max_n_) continue;
                State z = combine_pair(L[i].first, L[j].first, p, masks_);
                if (dead(z, p, masks_)) continue;
                clamp_pair(z, p);
                U128 w = L[i].second * L[j].second;
                out[canonical_pair(z)] += i == j ? w : 2 * w;
            }
        });
    }

    Table triple_stage(const Table& P, const Table& L, const Pass& p) {
        if (p.base()) masks_.prepare(mask_ids(P), mask_ids(L));
        return parallel_merge(P.size(), [&](size_t i, StateMap& out) {
            for (auto const& [y, v] : L) {
                if (P[i].first.k + y.k > max_n_) continue;
                State z = combine_triple(P[i].first, y, p, masks_);
                if (dead(z, p, masks_)) continue;
                clamp_triple(z, p);
                out[pack(z)] += P[i].second * v;
            }
        });
    }

    std::vector<U128> final_stage(const Table& R, const Table& H, const Pass& p) {
        if (p.base()) masks_.prepare(mask_ids(R), mask_ids(H));
        std::vector<std::vector<U128>> parts(threads_, std::vector<U128>(max_n_ + 1));
        std::vector<std::thread> pool;
        for (unsigned i = 0; i < threads_; ++i)
            pool.emplace_back([&, i] {
                for (size_t j = i; j < R.size(); j += threads_)
                    for (auto const& [h, v] : H) {
                        int n = R[j].first.k + h.k;
                        if (n <= max_n_ && final_alive(R[j].first, h, p, masks_)) parts[i][n] += R[j].second * v;
                    }
            });
        for (auto& th : pool) th.join();
        std::vector<U128> out(max_n_ + 1);
        for (auto const& part : parts) for (int n = 0; n <= max_n_; ++n) out[n] += part[n];
        return out;
    }

    int max_n_;
    unsigned threads_;
    Masks masks_;
    Table suits_, honours_;
    std::ostringstream log_;
};

// Shanten of one observed deal through the same transitions (without the
// mirror merge, which is only valid for aggregated weights). Used by tests.
inline int evaluate_deal(const std::array<int, 34>& counts, unsigned routes, Masks& masks) {
    Solver suit_solver(9, true), honour_solver(7, false);
    std::array<State, 3> suits;
    for (int s = 0; s < 3; ++s) {
        std::array<uint8_t, 9> c{};
        for (int i = 0; i < 9; ++i) c[i] = uint8_t(counts[9 * s + i]);
        suits[s] = suit_summary(c, suit_solver, masks);
    }
    std::array<uint8_t, 9> hc{};
    for (int i = 0; i < 7; ++i) hc[i] = uint8_t(counts[27 + i]);
    State honour = honour_summary(hc, honour_solver, masks);
    for (int t = -1; t <= 12; ++t) {
        Pass p{t, routes};
        std::array<State, 3> s = suits;
        State h = honour;
        for (auto& x : s) restrict_routes(x, p, masks);
        restrict_routes(h, p, masks);
        if (dead(s[0], p, masks) || dead(s[1], p, masks) || dead(s[2], p, masks) || dead(h, p, masks)) return t;
        masks.prepare({s[0].mask}, {s[1].mask});
        State pr = combine_pair(s[0], s[1], p, masks);
        if (dead(pr, p, masks)) return t;
        clamp_pair(pr, p);
        masks.prepare({pr.mask}, {s[2].mask});
        State tr = combine_triple(pr, s[2], p, masks);
        if (dead(tr, p, masks)) return t;
        clamp_triple(tr, p);
        masks.prepare({tr.mask}, {h.mask});
        if (!final_alive(tr, h, p, masks)) return t;
    }
    return 13;
}

}  // namespace gb
