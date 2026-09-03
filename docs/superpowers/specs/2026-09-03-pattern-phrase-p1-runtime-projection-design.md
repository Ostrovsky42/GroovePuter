# 0.9.10 P1 — Immutable Runtime Event Projection Design

## Status

Proposed production checkpoint after frozen P0 (`PR #426`).

## Goal

Route the current authoritative PATTERN source through a fixed-size,
read-only runtime projection consumed by existing Synth A/B playback, while
preserving all PATTERN behavior.

## Scope

P1 establishes only this boundary:

```text
authoritative PATTERN -> immutable runtime projection -> existing playback
```

The initial projection is PATTERN-only. It makes no PHRASE data authoritative
and does not add another scheduler or playback path.

## Runtime representation

Each Synth voice owns a fixed pair of projection slots. A projection contains:

- immutable event data copied from the selected `SynthPattern`'s 16 steps;
- the source identity needed to distinguish Synth A/B and the selected Pattern
  target;
- no pointer to mutable Scene pattern storage;
- no heap-backed container, persistence field, Undo receipt, MIDI state, or
  lifecycle state.

The event data preserves every `SynthStep` playback field used today, including
note, velocity, accent, slide, timing and existing per-step performance data.
P1 does not reinterpret steps as explicit PHRASE `startTick`/`durationTicks`.

The active slot is published atomically after the inactive slot is fully
prepared. Playback reads only the published immutable slot; it does not build,
allocate, compare, or edit projection data in the audio callback.

## PREPARE / COMMIT flow

```text
control-side source snapshot
  -> build fixed projection slot (PREPARE)
  -> AudioMutationGate control scope
  -> publish prepared inactive slot (COMMIT)
  -> audio callback consumes published read-only slot
```

Projection preparation occurs outside the audio callback. Publication uses the
existing `AudioMutationGate` control-side mutation topology and release/acquire
publication. The callback never waits, takes a new lock, serializes, allocates,
or runs a source lookup to build a projection.

## Playback integration

`MiniAcid` playback sites that currently read `activeSynthPattern()` must read
the active projection instead. This includes normal step triggering and
retrigger behavior. The projection supplies the same step values, so its first
accepted behavior is observationally identical to current PATTERN playback.

The existing accepted quantized-generation pending audible overlay remains a
valid PATTERN source. It is projected through the same P1 path rather than
creating a second runtime event source.

## Refresh ownership

P1 refreshes the runtime projection at existing control-side PATTERN source
transitions only:

- PATTERN step edits and Pattern mutations;
- Synth bank/pattern selection;
- Song row/position selection that changes an active Synth pattern;
- accepted generation publication and its pending audible activation/settlement;
- normal init/reset/start paths that establish current audible PATTERN state.

The refresh is derived runtime settlement. It does not dirty the Scene, create a
Scene revision, publish an Undo receipt, or write persistence.

## Explicit exclusions

P1 must not add or change:

- PHRASE persistent note storage, `MAKE PHRASE`, PATTERN xor PHRASE ownership,
  Scene format/versioning, or Phrase Undo;
- GRID 1/8, 1/16, or 1/32 editor semantics;
- UI source switching or Tab behavior;
- scheduler timing, Song traversal, cross-bar physical sustain, note lifetime,
  internal-synth release policy, USB/DIN MIDI output, or MIDI ownership;
- a second queue, scheduler, audio task, mutation owner, or Undo owner.

## Failure and capacity policy

All projection capacity is compile-time fixed for the current 16-step PATTERN
model. Invalid source identity or unavailable slot causes the control-side
commit to fail closed and retain the last published projection. It must never
fall back to constructing a projection in the audio callback.

## Tests and acceptance

TDD begins with host tests that prove:

1. a projection copies the complete current PATTERN event data and remains
   independent from later source mutation;
2. publishing changes the playback-visible projection only after a prepared
   commit;
3. each Synth voice remains isolated;
4. existing PATTERN normal-step and retrigger behavior remain unchanged;
5. generation pending/activation paths remain PATTERN-compatible.

Source-contract tests reject prohibited production edits and require that the
audio rendering path consumes only already-published projection data.

Required final verification is the full existing matrix:

```text
host Core tests
SDL build
Cardputer ADV build
Cardputer ADV fixed-DRAM gate
SEQTRAK MIDI-only build
```

## Hardware assumptions

P1 introduces no hardware or wiring changes. Cardputer-ADV remains the target
with its existing audio configuration; SEQTRAK MIDI-only remains a build gate
only. No new I2C, GPIO, voltage, USB, DIN/UART, display, or MIDI endpoint is
used by this checkpoint.
