# Guobiao Mahjong Shanten Analysis

Exact and reproducible calculations for Guobiao Mahjong best-14 shanten analysis.

The project contains the report and the independent route calculators used to produce it:

- `report.md`: the analysis report. Markdown tables use centered alignment.
- `HANDOFF.md`: current progress, known bottleneck, and continuation plan.
- `compute_all_not_connected.cpp`: exact all-not-connected route distribution for `N=14..136`.
- `compute_combo_dragon.cpp`: exact pure combination-dragon route distribution for `N=14..37`. It excludes hands already covered by the all-not-connected route and uses absorbing winning states.
- `compute_n14_tianhe.cpp`: exact 14-tile Guobiao winning-pattern enumeration and inclusion-exclusion check.
- `sample_best14.cpp`: fixed-seed development sampler; its output is not an exact probability result.
- `shanten_core.hpp`: shared local decomposition and entity-weight utilities.
- `all_not_connected_distribution.txt`: exact all-not-connected route entity weights.
- `combo_dragon_distribution.txt`: exact pure combination-dragon route entity weights.

## Build

```bash
clang++ -O3 -std=c++17 compute_all_not_connected.cpp -o compute_all_not_connected
clang++ -O3 -std=c++17 compute_combo_dragon.cpp -o compute_combo_dragon
clang++ -O3 -std=c++17 compute_n14_tianhe.cpp -o compute_n14_tianhe
clang++ -O3 -std=c++17 sample_best14.cpp -o sample_best14
```

Run the exact route calculators:

```bash
./compute_all_not_connected > all_not_connected_distribution.txt
./compute_combo_dragon > combo_dragon_distribution.txt
./compute_n14_tianhe
```

The distribution files contain entity weights. For an observed sample size `N`, divide by `C(136,N)` to obtain probabilities. The current all-not-connected program uses `unsigned __int128`; its saved rows overflow the accumulator at `N=55..81` and require a wider exact integer before use. The combination-dragon range stays below this limit.

The report distinguishes independent route distributions from the final five-route minimum. Independent route tables cannot be added or minimized column by column to recover the joint distribution because route events overlap.

The report is currently complete for the documented marginal routes and the
exact `N=14` five-route union, but the exact joint five-route distribution for
`N>14` is still unfinished. See `HANDOFF.md` before extending the calculators.

## Validation

The source calculations use the 136-tile physical sample space, with four physical copies of each of the 34 tile types. Total-weight checks pass for the saved all-not-connected rows outside `N=55..81`; the overflow range is recorded in `HANDOFF.md` and remains an open implementation issue.
