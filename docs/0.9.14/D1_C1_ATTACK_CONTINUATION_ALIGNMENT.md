# 0.9.14 D1-C1 — Attack vs continuation semantic alignment

Status: CLOSED. Base: `f5d603ed` (D1-C). Scope: one semantic correction to the
P0 generated-Synth-A preservation evaluator. No new evidence, no new claim.

## The mismatch D1-C found and then partly mis-solved

`adaptTonalPlanToSynthPattern()` materializes

* `tonalPlan.onsets`        as **attack** SynthSteps, and
* `tonalPlan.continuations` as **extra physical** SynthSteps that carry the active
  note with `slide = true`.

So the Pattern → Runtime projection legitimately contains events at
`onsets | continuations`. That is a *representation fact*. It does not make a
continuation a new musical onset.

D1-C used `physicalSkeleton = onsets | continuations` not only to validate the
projection but as the subject of R2/R3 (every physical step had to be unchanged,
and R3 inherited the attack pitch class onto continuation steps). That overstated
the frozen contract:

| Claim | Frozen meaning | D1-C did | D1-C1 does |
|---|---|---|---|
| R2 | bass **attack** topology | whole physical skeleton must be unchanged | every origin attack present at its own step; candidate tick compared **at attack ordinals only** |
| R3 | pitch class **at attack** | also checked continuation steps (inherited pc) | attack steps only |

## Exact rule (implemented in `src/dsp/p0_preservation_evaluator.h`)

* `attackMask = origin.bassRhythm.onsets`, `continuationMask = ...continuations`,
  `physicalSkeleton = attackMask | continuationMask` — **projection evidence only**.
* Every projected CURRENT event is classified by its authoritative `sourceSteps[]`
  entry: origin attack / known origin continuation / neither.
* **R2**
  * an origin attack step with no CURRENT event at that step (missing or moved) → **FAIL**
    (direct contradiction);
  * a candidate attack tick that differs → **FAIL**;
  * `candidate.count != source.count` (no attack correspondence exists) → **UNKNOWN**;
  * a CURRENT event that is neither an origin attack nor a known continuation → **UNKNOWN**
    (meaning is not invented);
  * continuation steps may be absent, added-as-known, re-articulated, retimed → no effect.
* **R3** (only when R2 is PASS) compares CURRENT and candidate pitch class at
  **attack steps** with `origin.bassPitchClasses`. Continuation steps are ignored.
* R1 and the aggregation are unchanged: Pass iff R1 ∧ R2 ∧ R3, otherwise **Unknown,
  never Fail, never NEW_IDEA**.

`originPitchClassAt()` (continuation pitch inheritance) was removed: it was an owner
materialization fact, not part of R3.

## Behaviour changes versus D1-C

| Case | D1-C | D1-C1 |
|---|---|---|
| THIN (count changes) | R2 FAIL | R2 **UNKNOWN** (no correspondence to compare) |
| added event on a rest step | R2 FAIL | R2 **UNKNOWN** (unclassifiable) |
| removed/moved **attack** | R2 FAIL | R2 FAIL (unchanged) |
| continuation representation change (tick, duration, flag, removal) | R2 FAIL / could fail | R2/R3 PASS, CONTINUES |
| chromatic change on a continuation only | R3 FAIL | R3 PASS (not an attack) |
| chromatic change on an attack | R3 FAIL | R3 FAIL (unchanged) |

## Witnesses (`tests/test_0_9_14_d1c1_attack_continuation.cpp`, real P1R Reggae bar
with `attacks=8000 continuations=7000`)

A untouched bar → CONTINUES · B continuation retimed / removed / slide-toggled →
R2 PASS, R3 PASS · C moved attack → R2 FAIL, R3 UNKNOWN · D missing attack → R2 FAIL ·
E correspondence lost → UNKNOWN (not FAIL) · F chromatic on continuation → R3 PASS ·
G chromatic on attack → R3 FAIL · H 180 single-event mutations: never `Fail` summary,
never NEW_IDEA · I publication neutrality on five operations (byte/functionally identical
with and without the observation).

Mutants verified to fail: comparing continuation ticks (B1) and including continuation
steps in R3 (F).

## Firewall

`tests/test_0_9_14_d1c1_source_regressions.py` pins: distinct attack/continuation masks,
R2 skips continuation ticks, lost correspondence is UNKNOWN, R3 is attack-only, no
whole-skeleton equality, aggregate never `Fail`, `NewIdea` never named by the provider.

## Non-effects

`NEW_IDEA` paths from this provider: **0**. Publication effect: **0** (DevelopmentDisposition,
`prepareNextMelody`, NEXT eligibility, GO, anchors, toasts are untouched; the observation is
read-only and optional).
