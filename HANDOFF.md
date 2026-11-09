# Guobiao Shanten Analysis Handoff

## 1. Scope

This repository is a standalone working copy of the current Guobiao Mahjong
shanten analysis files from:

`/Users/chubbypig/Library/CloudStorage/OneDrive-个人/archive/雀魂相关/各种情况下的大数据分析/向听数分析/国标麻将/`

The source directory was not edited. The report and route calculators were
copied into this repository so that the calculation code, saved weights, and
documentation can be continued together.

## 2. Current progress

The following parts are complete and locally reproducible:

- The 136 physical-tile sample space and entity-weight model are documented.
- The ordinary-shape, seven-pairs, and thirteen-orphans baseline is documented.
- The all-not-connected route has a saved marginal distribution for
  `N=14..136` in `all_not_connected_distribution.txt`. Its values pass the
  total-weight check through `N=54` and again from `N=82` onward, but the
  current `unsigned __int128` accumulator overflows for `N=55..81`; those
  rows must be regenerated with a wider integer type before they can be
  called exact.
- The pure combination-dragon route has an exact marginal distribution for
  `N=14..37` in `combo_dragon_distribution.txt`. It removes the overlap with
  all-not-connected and treats completed states as absorbing.
- For `N=14`, the five-route winning-hand union has been checked exactly. The
  union count is `13,257,769,189,488`, giving approximately
  `0.000311925122985409%` or `1/320589.76`.
- The report records the exact minimum formulation on one observed tile set,
  including the fact that the minimum may be exchanged with the route minimum
  only before marginalisation.

## 3. Main unfinished part

The report is intentionally marked **joint calculation incomplete**. The
tables for the baseline, all-not-connected, and combination-dragon routes are
independent marginal distributions. They cannot be added, subtracted, or
minimised column by column to obtain the five-route distribution, because the
same observed `N`-tile set can belong to several route events.

For an observed set `C`, the required quantity is the joint minimum

`S_N(C) = min(S_N,GQK(C), S_N,BL(C))`,

where the baseline value includes ordinary shape, seven-pairs, and
thirteen-orphans, and the special value includes all-not-connected and
combination-dragon. For every `N>14`, the repository does not yet contain an
exact table of this joint minimum. Consequently, `report.md` does not claim a
complete five-route `N>14` distribution.

## 4. Bottleneck found during the unified attempt

An exploratory unified state calculation was attempted with one global state
carrying the baseline decomposition and both special-route summaries. A local
suit state space was about 28,133 states in that experiment. Combining the
first two suits already produced more than ten million global states, and the
third-suit convolution did not finish within an acceptable run time. No
unverified probability was published from that attempt.

The difficulty is therefore the joint state space and the need to preserve
correlations, rather than a missing formula for either marginal route.

## 5. Recommended next implementation

The next implementation should calculate the joint result directly. A useful
design is to keep, for each partial physical-tile state:

1. the best baseline distance;
2. the best all-not-connected distance;
3. the best pure combination-dragon distance;
4. only the threshold/event information needed for the requested shanten
   histogram.

Dominance pruning should discard a partial state when another state has no
   worse values in every retained component and is at least as useful for all
   remaining tile counts. The transition must still retain physical-copy
   weights, and completed special-route states must remain absorbing. If a
   sparse joint correction is used instead, it must explicitly enumerate the
   affected sets satisfying

`S_N,BL(C) < S_N,GQK(C)`

and preserve their joint weight; marginal tables alone are insufficient.

## 6. Validation requirements for a future completion

- Every exact distribution must sum to `C(136,N)` for its stated `N` range.
  In particular, the all-not-connected implementation currently fails this
  check at `N=55..81` because its integer width is insufficient; replacing
  `U128` with `boost::multiprecision::cpp_int` (or an equivalent wider exact
  accumulator) is required before using those rows.
- The `N=14` five-route union must remain
  `13,257,769,189,488`.
- The final histogram must be computed from the per-observation minimum, not
  from independently aggregated route histograms.
- The physical four-copy limit must be enforced in every transition.
- Any sampling result must be labelled as diagnostic and must not replace the
  exact joint result.
- Markdown formulas should define symbols in prose and contain no Chinese
  text; table columns should remain centered with `:---:`.

## 7. Reproduction commands

From this directory:

```bash
clang++ -O3 -std=c++17 compute_all_not_connected.cpp -o compute_all_not_connected
clang++ -O3 -std=c++17 compute_combo_dragon.cpp -o compute_combo_dragon
clang++ -O3 -std=c++17 compute_n14_tianhe.cpp -o compute_n14_tianhe
clang++ -O3 -std=c++17 sample_best14.cpp -o sample_best14
./compute_all_not_connected > all_not_connected_distribution.txt
./compute_combo_dragon > combo_dragon_distribution.txt
./compute_n14_tianhe
```

The first two commands regenerate the saved exact marginal weights. The
sampler is fixed-seed development code and is not an exact probability
calculator.

## 8. Repository contents

- `report.md`: current analysis and explicit completion boundary.
- `compute_all_not_connected.cpp`: exact all-not-connected marginal route.
- `compute_combo_dragon.cpp`: exact pure combination-dragon marginal route.
- `compute_n14_tianhe.cpp`: exact `N=14` winning-pattern enumeration.
- `sample_best14.cpp`: fixed-seed development sampler.
- `shanten_core.hpp`: shared decomposition and weight utilities.
- `all_not_connected_distribution.txt`: saved all-not-connected weights.
- `combo_dragon_distribution.txt`: saved pure combination-dragon weights.

