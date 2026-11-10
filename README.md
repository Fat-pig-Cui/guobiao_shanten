# Guobiao Mahjong Shanten Analysis

Exact best-14 shanten distributions for Guobiao Mahjong: a player observes `N >= 14` tiles from the 136-tile set and keeps the best 14. Shanten is the minimum over ordinary shape, seven pairs, thirteen orphans, all-not-connected, and combination dragon, taken on the same observed tile set.

Base routes guarantee a win for `N >= 38` and all five routes for `N >= 37`, so the files cover `N=14..37`.

## Files

- `report.md`: the analysis report (Chinese), generated from `report_template.md`. Markdown tables use centered alignment and have at most seven columns.
- `report_template.md`: report text with one `{{table:NAME}}` line per table. Edit this file, not `report.md`.
- `HANDOFF.md`: method notes, validation status, and possible extensions.
- `guobiao_engine.hpp`: joint threshold engine for any subset of the routes.
- `compute_joint.cpp`: writes exact distributions from the engine.
- `test_joint.cpp`: randomised per-deal comparison against direct target enumeration plus aggregate checks.
- `compute_all_not_connected.cpp`, `compute_combo_dragon.cpp`: independent single-route calculators used for cross-checks.
- `compute_n14_tianhe.cpp`: exact 14-tile winning-pattern enumeration and inclusion-exclusion check. Its `N_COMBO` also includes all-not-connected hands that contain a full combination-dragon skeleton; `N_COMBO_ALL_INTER` is that part.
- `sample_best14.cpp`: fixed-seed sampler; its combination-dragon value is an approximation, so its output is diagnostic only and is not used to validate the report.
- `shanten_core.hpp`: shared block decomposition and weight utilities.
- `make_report_tables.py`: builds the report tables from the weight files; `--render report_template.md report.md` writes the report with the placeholders replaced. Tables with eight or more columns are split by column into two tables.
- `joint_distribution.txt`: five-route joint weights.
- `base_distribution.txt`, `all_not_connected_distribution.txt`, `combo_dragon_distribution.txt`: single-route marginal weights.
- `base_anc_distribution.txt`, `base_combo_distribution.txt`: base routes plus one special route, used for the route comparison in the report.

Each weight file row is `N W(-1) W(0) ... W(13)`. Divide by `C(136,N)` to obtain probabilities. Every row sums to `C(136,N)`.

## Build and run

Requires a C++17 compiler with `unsigned __int128` support (Clang or GCC),
Python 3, and Make. No third-party libraries are required.

```bash
make -j4        # build all calculators, tests, and the diagnostic sampler
make test       # run the default tests and check that report.md is up to date
make report     # regenerate report.md after editing the template or weights
```

Override the compiler with `make CXX=clang++` if needed. Build outputs and
local system files are ignored by Git. `make check-report` checks the saved
weight totals and report generation without changing the report.

Equivalent commands for building the exact calculators and regenerating data:

```bash
clang++ -O3 -std=c++17 compute_joint.cpp -o compute_joint
clang++ -O3 -std=c++17 test_joint.cpp -o test_joint
clang++ -O3 -std=c++17 compute_all_not_connected.cpp -o compute_all_not_connected
clang++ -O3 -std=c++17 compute_combo_dragon.cpp -o compute_combo_dragon
clang++ -O3 -std=c++17 compute_n14_tianhe.cpp -o compute_n14_tianhe

./test_joint                                    # about 50 s
./compute_joint all 37   > joint_distribution.txt              # about 65 s on Apple M4
./compute_joint base 37  > base_distribution.txt
./compute_joint anc 37   > all_not_connected_distribution.txt
./compute_joint combo 37 > combo_dragon_distribution.txt
./compute_joint base+anc 37   > base_anc_distribution.txt
./compute_joint base+combo 37 > base_combo_distribution.txt
python3 make_report_tables.py --render report_template.md report.md
```

`compute_joint` accepts `all` or route names joined by `+` (`base`, `anc`, `combo`), e.g. `base+anc`. The maximum `N` is 54 because weights are `unsigned __int128` and `C(136,N) >= 2^128` for `N=55..81`; base routes already win for every deal from `N=38`.

## Cross-checks

```bash
./compute_all_not_connected 37 | cmp - all_not_connected_distribution.txt   # about 70 s
./compute_combo_dragon 16 | diff - <(head -3 combo_dragon_distribution.txt) # about 40 s
./compute_n14_tianhe                                                         # N_TOTAL 13257769189488
```

The independent combination-dragon program has a large state space; `N=16` is a practical check, and `N=20` already needs about 4 GB of memory.
