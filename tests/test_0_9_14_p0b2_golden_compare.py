#!/usr/bin/env python3
"""P0-B2 compatibility gate. Baseline: the golden captured after B1 and before any archetype was admitted.
Every bar that existed in the baseline must be bit-identical; new bars may appear only where an archetype
was newly admitted, and never for Acid or House."""
import csv, sys

EXCLUDED = ("Acid", "House")


def load(path):
    return {(r["genre"], r["ordinal"], r["level"], r["law"], r["bar"]): (r["function"], r["hash"])
            for r in csv.DictReader(open(path), delimiter="\t")}


base, now = load(sys.argv[1]), load(sys.argv[2])
fail = []
for key, value in base.items():
    if key not in now:
        fail.append(f"baseline bar disappeared: {key}")
    elif now[key] != value:
        fail.append(f"existing bar changed: {key} {value} -> {now[key]}")
new = [k for k in now if k not in base]
for key in new:
    if key[0] in EXCLUDED:
        fail.append(f"admission leaked into an excluded genre: {key}")
by = {}
for key in new:
    by.setdefault(key[0], set()).add(key[1])
print(f"  baseline bars unchanged: {sum(1 for k in base if k in now and now[k] == base[k])}/{len(base)}; new bars: {len(new)}")
for genre, ords in sorted(by.items()):
    print(f"  new bars in {genre}: identities {sorted(int(o) for o in ords)}")
if fail:
    for f in fail[:20]:
        print("FAIL:", f)
    sys.exit(1)
print("P0-B2 golden compatibility: PASS")
