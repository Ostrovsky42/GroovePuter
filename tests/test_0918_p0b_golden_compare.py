#!/usr/bin/env python3
"""0.9.18 P0-B golden: the per-bar dump (every lane, hashed) must equal the golden captured at the
0.9.18 release candidate exactly. No bar may change, appear or disappear.

The historical goldens (pre_b1, b1, b2_steppers) stay in tests/golden/ as the record of the P0-B
steps. 0.9.18 changed generation on purpose (genre idioms, idea deck, DnB), so they no longer gate.
To re-baseline after an intended change, regenerate with tests/run_0_9_14_p0b_tests.sh and
M0_P0B_REBASELINE=1, and say in the commit which bars changed and why."""
import csv, collections, sys


def load(path):
    return {(r["genre"], r["ordinal"], r["level"], r["law"], r["bar"]): (r["function"], r["hash"])
            for r in csv.DictReader(open(path), delimiter="\t")}


golden, now = load(sys.argv[1]), load(sys.argv[2])
fail = []
fail += [f"bar disappeared: {k}" for k in golden if k not in now]
fail += [f"bar appeared: {k}" for k in now if k not in golden]
changed = collections.Counter()
for key, value in golden.items():
    if key in now and now[key] != value:
        changed[value[0]] += 1
        fail.append(f"bar changed: {key} {value} -> {now[key]}")
print(f"  bars unchanged: {sum(1 for k in golden if now.get(k) == golden[k])}/{len(golden)}")
for fn, n in sorted(changed.items()):
    print(f"  changed {fn}: {n}")
if fail:
    for f in fail[:20]:
        print("FAIL:", f)
    sys.exit(1)
print("0.9.18 P0-B golden: PASS")
