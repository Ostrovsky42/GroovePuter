#!/usr/bin/env python3
"""P0-B1 compatibility gate: compare the per-bar golden dump (every lane, hashed) with the
golden captured BEFORE B1. Frozen functions must be bit-identical; only Break, Reduction,
Build and Turnaround may change."""
import csv, collections, sys

FROZEN = ("Statement", "Repeat", "Return", "Response", "RepeatGhosts")
MUTABLE = ("Break", "Reduction", "Build", "Turnaround")


def load(path):
    rows = {}
    for r in csv.DictReader(open(path), delimiter="\t"):
        rows[(r["genre"], r["ordinal"], r["level"], r["law"], r["bar"])] = (r["function"], r["hash"])
    return rows


pre, post = load(sys.argv[1]), load(sys.argv[2])
fail = []
if set(pre) != set(post):
    fail.append(f"corpus keys differ: {len(set(pre) ^ set(post))} bars only in one dump")
stat = collections.defaultdict(lambda: [0, 0])
for key in pre:
    if key not in post:
        continue
    fn_pre, h_pre = pre[key]
    fn_post, h_post = post[key]
    if fn_pre != fn_post:
        fail.append(f"bar function changed at {key}: {fn_pre} -> {fn_post}")
        continue
    stat[fn_pre][0] += 1
    stat[fn_pre][1] += h_pre != h_post
for fn in FROZEN:
    if stat[fn][1]:
        fail.append(f"{fn}: {stat[fn][1]} of {stat[fn][0]} bars changed (must be 0)")
for fn in MUTABLE:
    if stat[fn][0] and not stat[fn][1]:
        fail.append(f"{fn}: no bar changed; B1 has no effect on it")
for fn, (n, c) in sorted(stat.items()):
    print(f"  {fn:14s} bars={n:4d} changed={c}")
if fail:
    for f in fail:
        print("FAIL:", f)
    sys.exit(1)
print("P0-B1 golden compatibility: PASS (frozen bar functions bit-identical over", sum(v[0] for v in stat.values()), "bars)")
