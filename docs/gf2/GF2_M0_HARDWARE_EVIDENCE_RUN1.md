# GF2-M0 — hardware evidence, run #1

Extracted `[MEM-PHRASE]` series from the first Cardputer ADV runtime capture.
The raw serial log is **not** committed here; this file is the manifest plus the
extracted table.

## Manifest

```text
source SHA        bf073e19c2b9b4f0ae7b4d751e334f657d6fbe60
branch            research/20260902-04-0.9.10-gf2-m0-transient-memory-invariant
image             normal runtime (memory-baseline diagnostic image)
FQBN              m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,
                  USBMode=default,CDCOnBoot=cdc,UploadMode=cdc
device            Cardputer ADV (ESP32-S3), USB 303a:1001
captured          2026-09-02 via scripts/monitor.sh 0 (continuous)
log span          ms=1280511; PHRASE burst completed by ms~744000
PHRASE attempts   46
successful        19
failed            27
heap integrity    0 violations

raw log           serial-continuous-20260902-183626.log
sha256            07eded98fe4274678847572a913609ae7deb2265b641e2ce8ea2b44026e27ecd
```

The raw log is retained outside the repository. Verify a copy with:

```bash
sha256sum -c serial-continuous-20260902-183626.log.sha256
```

## Result encoding

`result` is `static_cast<int>(GeneratedPhraseSong::LifecycleStatus)`:

```text
0 Failed          1 CommittedNow    2 PendingNextBar
3 Busy            4 TargetChanged   5 OutOfMemory
```

It records whether the operation succeeded, not why it failed. Failure-cause
classification is a separate diagnostic axis and is deliberately out of M0.

## Findings

### Heap: PHRASE is not a heap-pressure path

```text
preFreeInternal8 - localMinFreeInternal8

  32 B  x45
  40 B  x1     (seq=8, CommittedNow)
```

Successes and failures are indistinguishable at this resolution. The figure is
an upper bound: `localMinFreeInternal8` is system-wide for the window, so it
also absorbs whatever other tasks did concurrently.

`postFreeInternal8 == localMinFreeInternal8` in all 46 records, and the
per-operation `preFreeInternal8` range over the whole series is 4752..5476 with
the last value (5476) above the first (5136). No persistent loss.

### Fragmentation: no PHRASE-attributed drift

`largestInternal8` reads 2292 in all 92 probe samples, pre and post,
without a single deviation.

The periodic sampler separately recorded `minLargestInternal8RuntimeSample=1588`
and `minFreeInternal8Boot=2164` during the session, but 1588 was already reached
at ms=340202, before the PHRASE burst, during ordinary navigation. The probe
shows PHRASE's own local minimum never fell below 4720. Those system lows are
therefore not attributable to generation.

### Stack: the dominant transient resource

```text
seq=1   CommittedNow   19828 -> 14884   new low  -4944 B
seq=37  CommittedNow   14884 -> 14608   new low   -276 B

remaining free loop stack   14608 B
additional low-water        5220 B from idle
```

### The seq=37 caveat — read this before setting any threshold

The first successful generation established a 4944 B low-water mark, and the
next 35 operations added nothing. A deeper execution path only appeared on
operation 37, taking another 276 B.

A 20-operation capture, which is what the plan originally called for, would have
recorded 4944 B and presented it as the peak.

This series therefore proves:

```text
In this 744 s window, across the 46 execution paths exercised, the observed
worst-case additional loop-stack low-water was 5220 B.
```

It does **not** prove:

```text
PHRASE requires at most 5220 B.
```

That is an observed maximum, not a demonstrated upper bound. No release
threshold is frozen from this run.

## Series

Constant across every row: `preLargestInternal8 = postLargestInternal8 = 2292`.
Bold marks a new loop-stack low-water.

| seq | result | preFree | localMin | dip | postFree | stack pre → post |
|----:|:-------|--------:|---------:|----:|---------:|:-----------------|
| 1 | CommittedNow | 5136 | 5104 | 32 | 5104 | **19828 → 14884** |
| 2 | CommittedNow | 5328 | 5296 | 32 | 5296 | 14884 |
| 3 | Failed | 5200 | 5168 | 32 | 5168 | 14884 |
| 4 | Failed | 4752 | 4720 | 32 | 4720 | 14884 |
| 5 | Failed | 4752 | 4720 | 32 | 4720 | 14884 |
| 6 | Failed | 4752 | 4720 | 32 | 4720 | 14884 |
| 7 | Failed | 5136 | 5104 | 32 | 5104 | 14884 |
| 8 | CommittedNow | 5284 | 5244 | 40 | 5244 | 14884 |
| 9 | CommittedNow | 5476 | 5444 | 32 | 5444 | 14884 |
| 10 | CommittedNow | 5156 | 5124 | 32 | 5124 | 14884 |
| 11 | CommittedNow | 5476 | 5444 | 32 | 5444 | 14884 |
| 12 | Failed | 5156 | 5124 | 32 | 5124 | 14884 |
| 13 | Failed | 5028 | 4996 | 32 | 4996 | 14884 |
| 14 | Failed | 5028 | 4996 | 32 | 4996 | 14884 |
| 15 | Failed | 5028 | 4996 | 32 | 4996 | 14884 |
| 16 | Failed | 5028 | 4996 | 32 | 4996 | 14884 |
| 17 | Failed | 5028 | 4996 | 32 | 4996 | 14884 |
| 18 | Failed | 5028 | 4996 | 32 | 4996 | 14884 |
| 19 | Failed | 5284 | 5252 | 32 | 5252 | 14884 |
| 20 | Failed | 5028 | 4996 | 32 | 4996 | 14884 |
| 21 | Failed | 5028 | 4996 | 32 | 4996 | 14884 |
| 22 | CommittedNow | 5284 | 5252 | 32 | 5252 | 14884 |
| 23 | CommittedNow | 5476 | 5444 | 32 | 5444 | 14884 |
| 24 | CommittedNow | 5348 | 5316 | 32 | 5316 | 14884 |
| 25 | CommittedNow | 5412 | 5380 | 32 | 5380 | 14884 |
| 26 | Failed | 5476 | 5444 | 32 | 5444 | 14884 |
| 27 | Failed | 5028 | 4996 | 32 | 4996 | 14884 |
| 28 | Failed | 4900 | 4868 | 32 | 4868 | 14884 |
| 29 | Failed | 5156 | 5124 | 32 | 5124 | 14884 |
| 30 | Failed | 5284 | 5252 | 32 | 5252 | 14884 |
| 31 | Failed | 5476 | 5444 | 32 | 5444 | 14884 |
| 32 | CommittedNow | 5284 | 5252 | 32 | 5252 | 14884 |
| 33 | Failed | 5476 | 5444 | 32 | 5444 | 14884 |
| 34 | Failed | 4900 | 4868 | 32 | 4868 | 14884 |
| 35 | Failed | 4900 | 4868 | 32 | 4868 | 14884 |
| 36 | Failed | 4900 | 4868 | 32 | 4868 | 14884 |
| 37 | CommittedNow | 4960 | 4928 | 32 | 4928 | **14884 → 14608** |
| 38 | CommittedNow | 5476 | 5444 | 32 | 5444 | 14608 |
| 39 | CommittedNow | 5476 | 5444 | 32 | 5444 | 14608 |
| 40 | CommittedNow | 5476 | 5444 | 32 | 5444 | 14608 |
| 41 | Failed | 5348 | 5316 | 32 | 5316 | 14608 |
| 42 | CommittedNow | 5156 | 5124 | 32 | 5124 | 14608 |
| 43 | CommittedNow | 5156 | 5124 | 32 | 5124 | 14608 |
| 44 | CommittedNow | 5348 | 5316 | 32 | 5316 | 14608 |
| 45 | CommittedNow | 5348 | 5316 | 32 | 5316 | 14608 |
| 46 | Failed | 5476 | 5444 | 32 | 5444 | 14608 |
