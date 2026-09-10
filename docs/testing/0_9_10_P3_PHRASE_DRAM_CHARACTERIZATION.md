# 0.9.10-P3 — Phrase DRAM Hardware Characterization

Status: **P3 DRAM STEADY-STATE PASS WITH CHARACTERIZATION NOTES**

## Purpose

Freeze the completed Cardputer ADV hardware characterization for the P3 PHRASE runtime before product UX integration begins.

This document records measured static and runtime memory evidence only. It does **not** change the DRAM ceiling, reinterpret the retained deltas, or authorize production changes intended to "fix" characterization notes.

## Exact ancestry

```text
P3 characterization base
020582e19933e0c83bc62ad7a05bced2ceab2b23
```

Known P3 lineage:

```text
020582e1  test(p3): reach the phrase path from the diagnostic image only
48762854  feat(p3): schedule phrase events in phrase-relative time
21f03a66  feat(p3): own phrase material per synth voice
191e1655  fix(p3): make bounded phrase source contract truthful
2b0767a1  feat(p3): add bounded phrase sequenced source
```

Production semantic delta in this document: **NONE**

## Static product baseline

```text
normal product fixed DRAM   192904
provisional ceiling         191488
static excess               +1416
```

The provisional ceiling remains unchanged.

Known ELF identities:

```text
normal product:
0346bd89ea1b7fcbb87d95ee9126037a31fbe5ce5ea78252814a0340f37c2354

midi-only product:
46a204c523c3309ccf8c7a299c66280cd71e3448be9e73cb2bd249af5da81416

P3 normal diagnostic:
3709dd732bee298b272f6f0fad00a40112aabba4aed0cc8c442f7d8dfa56edb5
```

Diagnostic fixed DRAM:

```text
192984
```

The diagnostic scenario state added only:

```text
+16 B
```

versus the ordinary runtime-instrumented image.

## Product diagnostic firewall

The product firewall was proven independently by fixed-DRAM identity and ELF marker exclusion:

```text
product fixed before injection = 192904
product fixed after injection  = 192904

P3 scenario markers in product ELF = 0
P3 scenario markers in diagnostic ELF > 0
```

The normal product therefore remained byte-budget neutral with respect to the diagnostic scenario injection path.

Static attribution discovered during characterization:

```text
g_miniAcidInstance delta   +2568 B
pre-P3 reserve             +1144 B
```

These values are recorded as measured attribution only and are not reinterpreted here.

## Hardware measurements

```text
phase                 freeInt   minFreeInt   largestInt   integrity

periodic pre-P3       27292     27292        16372        1
p3-playback-start     27292     27292        16372        1
p3-cross-bar          26144     25792        16372        1
p3-peak #1            26144     25532        16372        1
p3-stop-1             26144     25532        16372        1
p3-restart-1          26144     25532        16372        1
p3-peak #2            25688     25532        16372        1
p3-stop-2             26144     25532        16372        1
p3-restart-2          26144     25532        16372        1
p3-peak #3            26008     25044        16372        1
p3-stop-3             26008     25044        16372        1
```

## Conservative interpretation

```text
heap integrity                  PASS
largest-block degradation       NOT OBSERVED
per-cycle accumulation          NOT OBSERVED
steady-state cleanup            PASS for proven cycles

first-playback retained delta   1148 B
                                attribution UNKNOWN

late stop variance              136 B
                                unexplained/non-reproducible
```

The `1148 B` first-playback retained delta is **not** attributed definitively to PHRASE. The evidence does not establish that ownership.

The `136 B` late stop variance is **not** classified as a leak. It was unexplained and non-reproducible in the characterized run.

No largest-internal-block degradation was observed across the proven cycles. No per-cycle monotonic accumulation was observed. Heap integrity remained valid in every recorded phase.

## Closure

```text
P3 DRAM STEADY-STATE PASS
WITH CHARACTERIZATION NOTES
```

This closure means only:

- the characterized P3 runtime path survived the measured playback / stop / restart cycles;
- heap integrity passed for the measured phases;
- no progressive largest-block degradation was observed;
- no per-cycle accumulation was observed in the proven cycles;
- the existing `+1416 B` static excess above the provisional `191488` ceiling remains inherited static debt and is not reclassified by this characterization.

It does **not** mean:

- the DRAM ceiling may be raised;
- the `1148 B` retained delta has a known PHRASE cause;
- the `136 B` sample is a proven leak;
- production should be modified merely to erase either characterization note.

## Product reachability context

At this P3 closure point the PHRASE playback path is intentionally reachable in the diagnostic image, while product UX still has no legitimate route that selects and edits PHRASE material.

That separation is the starting condition for P3-U1:

```text
before P3-U1
product PHRASE path reachable = NO

after P3-U1 target
product PHRASE path reachable = YES
```

The diagnostic firewall remains a required invariant during P3-U1: diagnostic scenario symbols and phase strings must remain absent from the normal product image.
