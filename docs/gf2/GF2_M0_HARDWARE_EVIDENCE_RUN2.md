# GF2-M0 — hardware evidence, run #2 (cold boot #2)

Second Cardputer ADV capture on the `normal` profile, after a full power-cycle
cold boot. Raw serial log is not committed; this is the manifest plus the
extracted analysis. Run #1: [GF2_M0_HARDWARE_EVIDENCE_RUN1.md](GF2_M0_HARDWARE_EVIDENCE_RUN1.md).

## Manifest

```text
source SHA        564b279f66172e8d93725c641f93823e52f3e76c
image             normal runtime (flash 1302814, DRAM globals 187040)
device            Cardputer ADV (ESP32-S3), USB 303a:1001
boot              full power-cycle cold boot
log span          ms=392188
scenario          idle -> navigation -> PLAY -> STOP -> edit -> PHRASE burst
                  -> navigation/PLAY/STOP/edit -> PHRASE tail

PHRASE attempts   93
  CommittedNow    31      PendingNextBar   3
  Failed          33      Busy             7
  TargetChanged   19      OutOfMemory      0
successes         34      (operator bool: CommittedNow + PendingNextBar)
heap integrity    0 violations

raw log           serial-continuous-20260902-210201.log
sha256            ba0818e6bb7681e1bd96158e435697f0af052ea6f85c21e0fe8e483eedcb0df0
```

## Stack: replicated to the byte

```text
run #1   19828 -> 14884 (seq=1)                    -> 14608 (seq=37)
run #2   19828 -> 14884 (seq=1) -> 14880 (seq=10)  -> 14608 (seq=16)
```

Both runs enter PHRASE at 19828 free loop stack, despite different
`runtime-start` values (27092 / 27188): UI navigation brings the loop task to
the same value deterministically. Both make the same first step of -4944 B and
converge on **14608**, i.e. 5220 B of additional low-water, by different
trajectories and after a different number of operations.

`largestInternal8` at `runtime-start` also matched exactly across boots: 3060.

This makes 5220 B a **replicated observed maximum**. It is still not a formal
upper bound - run #2 found a new intermediate step of just 4 bytes at seq=10,
which is direct evidence that distinct execution paths differ by single-byte
amounts and that path space is not enumerated by soaking.

## Heap: the finding that changed after run #1

Run #1 sampled only two lifecycle branches. Run #2 exercised five, because the
scenario included PLAY and navigation.

```text
seq=84  TargetChanged    pre=5404  localMin=5372  dip=32
seq=85  PendingNextBar   pre=5468  localMin=4912  dip=556
seq=86  Busy             pre=5468  localMin=5436  dip=32
```

92 of 93 records dip exactly 32 B. One - the `PendingNextBar` path, which did
not occur at all in run #1 - dips **556 B**, seventeen times the baseline.

Therefore the run #1 phrasing "PHRASE heap pressure is about 32 B" is retired.
The measured statement is:

```text
most observed PHRASE lifecycle paths   32 B system-local heap dip
observed PendingNextBar path           up to 556 B, n=3
persistent heap loss                   none observed
PHRASE-attributed largest-block drift  none observed
```

That 556 B fits inside the 2292 B largest free block is an observation, not a
rule. A later lifecycle path could dip 900 or 1400 B, and that would still not
make 2292 a correct budget.

### What `post = pre - 32` does and does not mean

In every record across both runs, including seq=85, the post-operation sample
returns a footprint of 32 B (40 B once). For seq=85 this means the roughly
524 B **above the baseline 32 B** were released by the time of the post sample,
leaving the ordinary footprint. It does not mean the whole 556 B was measured
returning inside the end hook - the probe samples two points, not a curve.

## Lifecycle census, both runs (139 operations)

`largestInternal8` reads 2292 in every sample of every branch. All five loop
stack low-water moves came from `CommittedNow`.

| lifecycle branch | run #1 | run #2 | max heap dip | stack low-water moves | largestInternal8 |
|:--|--:|--:|--:|--:|:--|
| Failed | 27 | 33 | 32 B | 0 | 2292 |
| CommittedNow | 19 | 31 | 40 B | 5 | 2292 |
| PendingNextBar | 0 | 3 | 556 B | 0 | 2292 |
| Busy | 0 | 7 | 32 B | 0 | 2292 |
| TargetChanged | 0 | 19 | 32 B | 0 | 2292 |

The post-sample footprint takes only two distinct values across all 139
operations: 32 B and 40 B.

## Why the old stop condition was insufficient

Run #2 stopped at 22 consecutive successes since the last low-water, short of
the required 25. But that is the lesser problem.

The saturation condition measures the depth of the *successful* generation
path. It says nothing about lifecycle coverage. Run #2 satisfied every count in
the original protocol - 93 attempts, 34 successes, well past the 50/20 minimum -
and still sampled `PendingNextBar` exactly three times, one of which produced
the only non-baseline heap behaviour observed so far.

Run #3 therefore replaces soak duration with branch coverage.
