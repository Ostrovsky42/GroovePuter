# P3 — Same-Chord ChordRhythm Retrigger

**Base:** `fc42763e7798866e61895bf1b8d62339ec59e0a7`  
**Scope:** semantic capability prototype only; no H6 vocabulary or live migration

## Question

Can ChordRhythm represent a new audible chord attack that reuses the current harmonic source event, without treating it as continuation, without consuming the next source event, and without inventing gate policy?

## Current Stage15 fact

The one-bar `ChordRhythmPlan` already owns three gate/action surfaces:

```text
onsets
continuations
releasePoints
```

Stab profiles have onsets without continuations; HeldPad has explicit continuation and release. Therefore same-chord retrigger must be a fourth **action distinction**, not a rule that automatically fills continuation steps.

An early P3 prototype inferred holds from audible onset topology and was rejected before freeze.

## Corrected contract

```text
sourceAdvanceOnset  N  -> audible onset, advance local source ordinal
sameChordRetrigger  S  -> audible onset, keep current source identity
continuation           -> no audible onset, preserve current gate
releasePoint           -> no audible onset, close current gate
```

Request:

```text
sourceAdvanceOnsets
sameChordRetriggers
continuations
releasePoints
sourceAvailableAtStart
```

Plan:

```text
audibleOnsets = sourceAdvanceOnsets | sameChordRetriggers
sourceAdvanceCount
audibleOnsetCount
sourceOrdinalByStep[16]
+ exact continuation/release masks
```

Normal local example:

```text
step        0   4   8   12
action      N   S   N    S
source      0   0   1    1
```

Retrigger increases audible onset count but not source-advance count.

## Explicit incoming source for later P2 composition

A future multi-bar composition may begin a bar with `S` while the current harmonic source identity was established in the previous bar. P3 does not read global, Scene or migration state to discover that condition.

Instead the caller may explicitly set:

```text
sourceAvailableAtStart = true
```

Then retriggers before the first local `N` are valid and `sourceOrdinalByStep` uses the reserved sentinel:

```text
kIncomingChordRhythmSourceOrdinal = 0xFE
```

After the first local `N`, local source ordinals begin at `0` as usual. Without `sourceAvailableAtStart`, a retrigger before the first local `N` is `OrphanRetrigger`.

This keeps cross-bar carry composable with P2 without creating a hidden harmonic-state owner.

## Gate semantics remain independent

P3 copies explicit continuation/release masks; it never derives them from N/S positions. A retrigger can therefore be a stab, start an explicit hold, or occur after an earlier release while still reusing the current harmonic source identity.

All four masks must be pairwise disjoint. A continuation requires an immediately active gate. A release requires an immediately active gate. A gap ends the gate but does not erase the current harmonic source identity, so a later retrigger after silence remains valid.

## Validation

- overlapping action/gate semantics are rejected;
- retrigger before any local source advance is rejected unless incoming source availability is explicitly supplied;
- incoming-source retrigger is marked with the INCOMING sentinel, not silently renumbered;
- orphan continuation/release topology is rejected;
- invalid requests fail closed;
- empty request is `ValidButEmpty`;
- one source advance plus 15 retriggers yields 16 audible onsets while all local attacks map to source ordinal 0.

## Ownership

P3 contains no harmonic event payload, degree, quality, MIDI note, synth destination, Genre, Scene or P-level routing. It never duplicates a progression event to create re-articulation.

P3 is intentionally one-bar and independent from P2. A later integration PR may lift the same N/S/gate semantics onto the P2 1..4-bar timeline after both capabilities freeze independently.

## Explicit exclusions

- no H6 rhythm masks/candidates;
- no H6 harmonic progressions;
- no ChordProgression edits;
- no production ChordRhythm profile edits;
- no multi-bar representation here;
- no synth/MIDI emission;
- no live Stage15 migration wiring.

## Acceptance

Host tests prove local N/S source ordinal mapping, explicit incoming-source carry, retrigger-vs-continuation separation, explicit continuation/release preservation, retrigger after release, orphan/overlap rejection, invalid gate topology, dense retrigger behavior, source-topology immutability and valid empty behavior.

ESP32-S3 normal and midi-only builds measure an isolated linker-retained P3 probe and prove ordinary product builds dead-strip it. Static measurements do not claim future live MIDI/audio lifecycle cost.
