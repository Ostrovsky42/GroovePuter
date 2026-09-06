# 0.9.10-MEMORY-R0 — HARDWARE EVIDENCE IDENTITY CONTRACT

Status: **FROZEN FOR R0 HARDWARE RUNS**  
Authoritative P3 source: `aded0e183a934f78623030226b67b5d0b598648b`  
Applies to: `MEMORY-R0-A / B / C / D` hardware evidence  
Production semantic delta: **NONE**

## Purpose

Prevent hardware observations from different firmware layouts, reset cycles, SD states, or interaction scripts from being compared as though they came from one experiment.

At the current sub-kilobyte runtime floor, diagnostic build drift can itself be material. Therefore every accepted R0 hardware result must carry the exact identity of the firmware and physical run beside the measured result.

## Mandatory evidence tuple

Record this tuple for **every** hardware run before interpreting memory deltas or failure causality:

```text
RUN ID
scenario                D0 / D1 / D2 / D3 / D4 / A1 / A2 / A3 / B-...
source commit           <git SHA>
instrumentation commit  <git SHA>
ELF path                <exact path>
ELF SHA-256             <sha256sum of exact flashed ELF>
reset reason            <boot-reported reason>
SD condition            absent / present
filesystem condition    <scenes/samples state relevant to run>
interaction             NONE or exact scripted actions
start condition         <cold boot / auto-reset after panic / other>
duration                <observed duration>
raw log                 <path>
```

For untouched-idle scenarios, `interaction` must be recorded literally as:

```text
NONE — no keys, encoder, page navigation, SMF load, sample selection, or other user input after boot
```

A result without firmware identity or interaction identity is contextual evidence only and must not be promoted to an authoritative R0 differential.

## Exact-ELF rule

The A/B/C/D hardware characterization should use one exact instrumentation ELF wherever the experiment allows it.

The following are **not** interchangeable merely because they were built from nearby source commits:

- different diagnostic probe sets;
- different logging strings or verbosity;
- different task composition;
- different compile-time diagnostic scenarios;
- rebuilt ELFs whose SHA-256 differs;
- product vs diagnostic images.

If an instrumentation change requires a new ELF, start a new evidence series and do not calculate causal phase deltas across the old/new ELF boundary.

## Required measurement identity

Place the evidence tuple immediately adjacent to each run result containing any of:

- panic / stability verdict;
- reset reason;
- free `INTERNAL|8BIT`;
- largest internal block;
- minimum free;
- heap integrity;
- task stack HWM;
- phase delta;
- page construction local minimum;
- SMF load/playback local minimum.

Do not keep firmware identity only in a distant build log.

## D0 acceptance discipline

An authoritative `D0 FAILS` result requires:

- SD state explicitly recorded;
- empty-scenes/reproducer filesystem state recorded;
- exact ELF SHA-256 recorded;
- reset reason recorded;
- `interaction = NONE` recorded;
- raw serial capture preserved.

An authoritative `D0 STABLE` result requires the same identity fields plus the observation duration.

This prevents a later interactive run, rebuilt diagnostic image, or different SD state from being mistaken for the untouched-idle experiment.

## Memory semantics

All R0 analysis must continue to distinguish:

```text
retained residency
    != construction peak
    != operation-local minimum
```

A before/after recovery does not erase a dangerous transient minimum. Example:

```text
before        1800 B
construction   350 B   <- critical local minimum
after         1500 B
```

The accepted result must retain the local minimum rather than reducing the operation to the net `300 B` before/after delta.

## Current freeze

```text
AUTHORITATIVE P3
aded0e183a934f78623030226b67b5d0b598648b

MEMORY-R1                     BLOCKED
MEMORY OPTIMIZATION           BLOCKED
PRODUCTION MEMORY CHANGES     NONE
```

No further source-only memory architecture work is required before the next hardware result.

The next evidence-producing action is the D0 raw panic capture with SD present and zero user input after boot.
