#!/usr/bin/env python3
"""Print the Markdown tables used in report.md from the saved weight files.

    python3 make_report_tables.py                                      # all tables
    python3 make_report_tables.py joint                                # one table by name
    python3 make_report_tables.py --render report_template.md report.md

The template holds the report text with one '{{table:NAME}}' line per table;
rendering replaces those lines, so report.md contains no marker syntax.
Tables with eight or more columns are split by column into two tables.

All arithmetic is exact (fractions); only the printed percentages round.
"""
import re
import sys
from fractions import Fraction
from math import comb

SHANTEN = list(range(-1, 14))


def load(path):
    rows = {}
    with open(path) as f:
        for line in f:
            r = list(map(int, line.split()))
            assert sum(r[1:]) == comb(136, r[0]), (path, r[0])
            rows[r[0]] = r[1:]
    return rows


MAX_COLUMNS = 7


def short(v):
    """Four significant digits; values below 0.001 use a compact exponent."""
    if abs(v) >= 0.001:
        return "%.4g" % v
    m, e = ("%.3e" % v).split("e")
    if "." in m:
        m = m.rstrip("0").rstrip(".")
    return "%se%d" % (m, int(e))


def pct(x):
    """Percentage of a Fraction in [0, 1]."""
    if x == 0:
        return "0"
    if x == 1:
        return "100%"
    p = float(x * 100)
    if p >= 99.99:
        return ">99.99%"
    return short(p) + "%"


def pp(x):
    """Difference of two probabilities in percentage points."""
    return "0" if x == 0 else short(float(x * 100))


def at_most(rows, n, r):
    return Fraction(sum(rows[n][: r + 2]), comb(136, n))


def exactly(rows, n, r):
    return Fraction(rows[n][r + 1], comb(136, n))


def mean(rows, n):
    return Fraction(sum(s * w for s, w in zip(SHANTEN, rows[n])), comb(136, n))


def header(cells):
    return ["| " + " | ".join(cells) + " |", "| " + " | ".join([":---:"] * len(cells)) + " |"]


def distribution(rows, cols, open_last=False):
    """Shanten distribution, split by column when it would exceed MAX_COLUMNS.

    Rows stop at the first all-win N. In the second part, trailing rows whose
    cells are all zero are dropped and a sentence states where zeros begin.
    """
    names = ["$%d$" % c for c in cols]
    if open_last:
        names[-1] = "$\\ge%d$" % cols[-1]
    table = []
    for n in sorted(rows):
        cells = []
        for i, c in enumerate(cols):
            if open_last and i == len(cols) - 1:
                cells.append(pct(1 - at_most(rows, n, c - 1)))
            else:
                cells.append(pct(exactly(rows, n, c)))
        table.append((n, cells))
        if exactly(rows, n, -1) == 1:
            break
    if len(cols) + 1 <= MAX_COLUMNS:
        parts = [range(len(cols))]
    else:
        half = (len(cols) + 1) // 2
        parts = [range(half), range(half, len(cols))]
    blocks = []
    for k, idx in enumerate(parts):
        body = [(n, [cells[i] for i in idx]) for n, cells in table]
        note = ""
        if k > 0:
            while len(body) > 1 and all(c == "0" for c in body[-1][1]):
                body.pop()
            if body[-1][0] < table[-1][0]:
                note = "\n\n$N\\ge%d$ 时以上各列均为 0。" % (body[-1][0] + 1)
        out = header(["$N$"] + [names[i] for i in idx])
        out += ["| %d | %s |" % (n, " | ".join(cells)) for n, cells in body]
        blocks.append("\n".join(out) + note)
    return "\n\n".join(blocks)


def main():
    joint = load("joint_distribution.txt")
    base = load("base_distribution.txt")
    base_anc = load("base_anc_distribution.txt")
    base_combo = load("base_combo_distribution.txt")
    anc = load("all_not_connected_distribution.txt")
    combo = load("combo_dragon_distribution.txt")
    tables = {}

    tables["joint"] = distribution(joint, list(range(-1, 7)))

    out = header(["$N$", "和牌形", "不超过听牌", "不超过一向听", "不超过两向听", "平均向听数", "基础路线平均向听数"])
    for n in sorted(joint):
        out.append("| %d | %s | %s | %s | %s | %.4f | %.4f |" % (
            n, *(pct(at_most(joint, n, r)) for r in (-1, 0, 1, 2)), float(mean(joint, n)), float(mean(base, n))))
    tables["indicators"] = "\n".join(out)

    out = header(["目标", "概率 ≥ 50%", "概率 ≥ 90%", "概率 ≥ 99%", "必然达到"])
    names = ["和牌形", "不超过听牌", "不超过一向听", "不超过两向听", "不超过三向听", "不超过四向听", "不超过五向听"]
    for r, name in zip(range(-1, 6), names):
        cells = []
        for p in (Fraction(1, 2), Fraction(9, 10), Fraction(99, 100), Fraction(1)):
            nj = min(n for n in joint if at_most(joint, n, r) >= p)
            nb = min([n for n in base if at_most(base, n, r) >= p] or [38])
            cells.append("%d（%d）" % (nj, nb))
        out.append("| %s | %s |" % (name, " | ".join(cells)))
    tables["needed"] = "\n".join(out)

    cols = list(range(-1, 5))
    out = header(["$N$"] + ["$\\le%d$" % r for r in cols])
    for n in sorted(joint):
        diffs = [at_most(joint, n, r) - at_most(base, n, r) for r in cols]
        if not any(diffs):
            break
        out.append("| %d | %s |" % (n, " | ".join(pp(d) for d in diffs)))
    tables["gain"] = "\n".join(out)

    for key, r in (("routes_win", -1), ("routes_tenpai", 0)):
        out = header(["$N$", "基础路线", "只加全不靠", "只加组合龙", "两者都加"])
        for n in range(14, 33, 2):
            b = at_most(base, n, r)
            out.append("| %d | %s | %s | %s | %s |" % (
                n, pct(b), pp(at_most(base_anc, n, r) - b), pp(at_most(base_combo, n, r) - b),
                pp(at_most(joint, n, r) - b)))
        tables[key] = "\n".join(out)

    tables["base"] = distribution(base, list(range(-1, 7)))
    tables["anc"] = distribution(anc, list(range(-1, 8)), open_last=True)
    tables["combo"] = distribution(combo, list(range(-1, 8)), open_last=True)

    if sys.argv[1:2] == ["--render"]:
        render(sys.argv[2], sys.argv[3], tables)
        return
    for name in sys.argv[1:] or list(tables):
        print("<!-- %s -->\n%s\n" % (name, tables[name]))


def render(template, output, tables):
    """Write output: template text with each '{{table:NAME}}' line replaced."""
    with open(template, encoding="utf-8") as f:
        lines = f.read().split("\n")
    out, seen = [], set()
    for line in lines:
        m = re.fullmatch(r"\{\{table:(\w+)\}\}", line.strip())
        if m:
            out.append(tables[m.group(1)])
            seen.add(m.group(1))
        else:
            out.append(line)
    text = "\n".join(out)
    assert "{{table:" not in text, "unreplaced table placeholder"
    with open(output, "w", encoding="utf-8") as f:
        f.write(text)
    print("rendered tables: " + ", ".join(sorted(seen)))


if __name__ == "__main__":
    main()
