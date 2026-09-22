# 0.9.10 P1 — Immutable Runtime Event Projection Design

## Status

Production design checkpoint after the P0 scheduler/lifetime characterization
line (`PR #426`). P1 production commits must not start until the accepted P0
characterization is available unchanged on the implementation base and its
focused workflow is terminal GREEN.

P1 is intentionally a runtime-boundary change only. It must preserve current
PATTERN sound, timing, Song traversal and generation activation behavior.

## Goal

Route the current authoritative PATTERN source through fixed-size, read-only
runtime projection data consumed by existing Synth A/B playback, while
preserving all accepted PATTERN behavior.

P1 creates the seam required for a later PATTERN xor PHRASE source model. It
does not make PHRASE authoritative yet.

## Scope

P1 establishes only this boundary:

```text
authoritative PATTERN material
        |
        v
control-side projection preparation
        |
        v
fixed resident immutable runtime projection
        |
        v
existing scheduler / existing playback
```

The initial projection is PATTERN-only. P1 adds no second scheduler, no second
audio path and no persistent runtime-event owner.

## Why a resident projection bank is required

A single current/next pair is insufficient to preserve existing behavior.
Current PATTERN selection can change at exact `BAR_START` inside the audio
execution path when Song advances a row, and the accepted generation audible
overlay is also released at that boundary.

Therefore P1 separates two operations that were previously implicit in
`activeSynthPattern()`:

1. **material preparation** — copying mutable Scene PATTERN bytes into immutable
   runtime projection data; this is never allowed in the audio callback;
2. **source selection** — choosing which already-prepared projection is audible;
   this may occur at an exact audio boundary and must remain constant-time.

## Runtime representation

### Page-resident PATTERN projection bank

Each Synth voice owns a fixed page-resident projection bank covering every
physical Synth PATTERN address in the currently resident page:

```text
2 banks x 8 PATTERN slots = 16 projection entries per Synth voice
```

This is compile-time fixed capacity. It does not scale with Song length and it
does not allocate one projection per Song row.

Each projection entry contains:

- immutable runtime step data derived from one `SynthPattern`'s 16 steps;
- every current `SynthStep` field used by playback: `note`, `slide`, `accent`,
  `ghost`, `velocity`, `timing`, `fx`, `fxParam` and `probability`;
- source identity sufficient to identify Synth A/B plus page, bank and slot;
- no pointer or reference to mutable Scene pattern storage;
- no heap-backed container, persistence field, Undo receipt, MIDI state,
  scheduler state or note-lifetime state.

The projection is a derived runtime value. Scene PATTERN storage remains the
single persistent PATTERN authority.

### Canonical empty projection

`NO PATTERN` is a valid musical source state, not an error.

A Song row with no Synth PATTERN selected must resolve to a canonical prepared
empty projection. It must become silent exactly where current
`activeSynthPattern()` resolves to `kEmptySynthPattern`.

`NO PATTERN` must never be handled by retaining stale audible material.

### Pending-generation audible overlay

The accepted quantized-generation contract commits new persistent PATTERN truth
before its exact `BAR_START` audible activation. Until that boundary, playback
must continue to hear the old captured PATTERN material.

P1 therefore keeps one fixed prepared audible-overlay projection per Synth
voice for the existing single pending generation owner. The overlay is derived
from the already captured old audible material during control-side generation
COMMIT. It is not a new queue and not a second generation owner.

While the accepted generation activation is pending:

```text
playback -> prepared old-audible overlay
```

At exact `BAR_START`, AudioTask only releases/claims already-published runtime
state and falls through to the resident committed projection:

```text
old overlay --BAR_START--> resident new committed PATTERN projection
```

No PATTERN copy, generation, Scene event-material lookup or allocation is
allowed at that boundary.

## PREPARE / PUBLISH / SELECT model

P1 has two publication classes because current source transitions originate on
both sides of the control/audio boundary.

### A. Control-originated material change

Used for PATTERN edits, Pattern mutation, page settlement, project/Scene
settlement and generation persistent COMMIT.

```text
control-side source snapshot
  -> build fixed projection value (PREPARE)
  -> existing AudioMutationGate control scope
  -> replace the affected resident projection entry (PUBLISH)
  -> audio resumes and reads only prepared projection data
```

Projection preparation occurs before entering the short publication portion
when practical. Publication of mutable runtime storage occurs only while the
existing AudioMutationGate has excluded concurrent AudioTask rendering, or
while AudioTask is not active during boot/reset settlement.

P1 adds no second mutex, spinlock or mutation gate.

### B. Audio-boundary source selection

Used when existing scheduler behavior changes the audible physical PATTERN at
an exact boundary, especially Song row advancement and pending-generation
activation.

```text
already-prepared resident/overlay projection
  -> exact existing BAR_START transition
  -> constant-time SELECT / RELEASE only
  -> existing playback continues
```

AudioTask may select an already-prepared projection or clear a prepared overlay.
It must not:

- copy `SynthPattern` or `SynthStep` material;
- build or refresh projection entries;
- allocate or free memory;
- wait, spin or take `AudioMutationGate`;
- serialize or access persistence;
- perform generation;
- resolve event material by dereferencing mutable Scene PATTERN storage.

Reading the small already-authoritative runtime source identity needed to select
a prepared entry is allowed. Event material itself must come only from the
projection.

## Page settlement

The projection bank mirrors the currently resident physical PATTERN page, not
all `kMaxPages` globally.

Existing page loading remains a control-side operation under the current audio
guard. When a new page has been loaded into Scene resident PATTERN storage, P1
must rebuild the fixed projection bank for that resident page before the new
page identity becomes playback-visible.

The page settlement invariant is:

```text
load/validate resident page
  -> prepare resident projection bank
  -> guarded publish of projection bank + page identity
  -> audio resumes
```

P1 does not add Song prefetching, a second page cache or a new page scheduler.
Any already-characterized page-switch timing remains compatibility behavior.

## Playback integration

Synth playback sites that currently obtain event material from
`activeSynthPattern()` must obtain it from runtime projection data instead.
This includes:

- microtiming lookup in `processSequencerEvents()`;
- normal step triggering in `triggerSynthStep_()`;
- Synth A/B retrigger reads in `generateAudioBuffer()`;
- audible Synth step caches where they intentionally represent the current
  audible runtime material.

The first accepted P1 behavior is observationally identical to current PATTERN
playback. P1 does not reinterpret PATTERN steps as explicit PHRASE
`startTick`/`durationTicks` events.

`activeSynthPattern()` may remain as a control-side/source-resolution helper if
needed, but no audio rendering path may depend on mutable Scene PATTERN event
material after P1.

## Refresh and selection ownership

P1 distinguishes **refresh** from **selection**.

### Refresh: event material changed

The affected resident projection entry or bank must be refreshed after existing
control-side PATTERN material transitions, including:

- individual PATTERN step edits;
- clear/randomize/rotate and other Pattern mutations;
- accepted generation persistent publication;
- project/Scene load and reset settlement;
- physical page load/creation settlement.

### Selection: audible physical source changed

Selection changes do not copy event material. They only point playback at an
already-prepared resident or empty projection. Existing selection transitions
include:

- Synth bank/pattern selection;
- Song position/row selection;
- exact Song row advancement inside AudioTask;
- mode transitions between Pattern and Song selection semantics;
- init/reset/start paths that establish the current audible source;
- pending-generation overlay publication, cancellation and exact-boundary
  release.

The projection is derived runtime settlement. Refresh or selection alone must
not dirty the Scene, create a Scene revision, publish an Undo receipt or write
persistence.

## Failure and capacity policy

All P1 capacity is compile-time fixed:

- 16 PATTERN projection entries per Synth voice for the resident page;
- one canonical empty projection;
- one fixed generation audible-overlay projection per Synth voice;
- fixed source metadata only.

No `std::vector`, heap allocation or unbounded queue is permitted for P1
projection state.

Failure rules are explicit:

- invalid mutation target or failed control-side preparation: fail closed and
  retain the last valid published projection for that same target;
- `NO PATTERN`: select canonical empty projection, never stale material;
- unavailable/invalid runtime selection identity: fail closed without building
  material in AudioTask and surface the condition to host characterization;
- failed page load: retain the previously accepted resident page and its
  projection bank under existing page-load rollback semantics;
- failed generation activation validation: preserve the existing generation
  cancellation/convergence policy; do not manufacture a projection fallback in
  AudioTask.

## P0 compatibility gate

P0 is the regression oracle for P1, not optional historical documentation.

Before P1 production implementation starts, the accepted P0 characterization
must be present unchanged on the implementation base. P1 acceptance must run
the focused P0 characterization runner unchanged in addition to new P1 tests.

The following P0 behaviors remain compatibility requirements unless a later,
separately reviewed checkpoint explicitly changes them:

- 384 ticks per physical bar and 24 ticks per PATTERN step;
- `gridSteps` remains a Synth scheduler no-op in P1;
- exact Song row boundary behavior;
- existing asymmetric internal-synth/MIDI cleanup behavior at Song transition;
- legacy `note == -2` TIE behavior;
- accepted swing + microtiming wrap behavior;
- identical Synth A/B behavior and current USB/DIN MIDI endpoint traces.

## Explicit exclusions

P1 must not add or change:

- PHRASE persistent note storage, `MAKE PHRASE`, PATTERN xor PHRASE ownership,
  Scene format/versioning, or Phrase Undo;
- GRID 1/8, 1/16, or 1/32 editor semantics;
- UI source switching or Tab behavior;
- scheduler tick math, Song traversal rules, cross-bar physical sustain, note
  lifetime, internal-synth release policy, USB/DIN MIDI output, or MIDI
  ownership;
- a second queue, scheduler, audio task, page cache, mutation owner or Undo
  owner.

## Tests and acceptance

TDD begins with host tests that prove the pure projection contract before
playback is rewired.

Required P1 behavior tests:

1. a projection copies every current Synth playback field and remains
   independent from later source mutation;
2. every resident page bank/slot maps to the correct fixed projection entry for
   both Synth voices;
3. `NO PATTERN` selects the canonical empty projection and cannot leak the
   previously audible PATTERN;
4. a control-side publish changes runtime event material only after the
   prepared/guarded publication point;
5. each Synth voice remains isolated;
6. Song row advancement selects already-prepared projection material at the
   exact existing boundary without copying Pattern material in AudioTask;
7. normal step triggering, probability/ghost behavior, timing and retrigger
   behavior remain unchanged;
8. generation pending publication keeps old audible material until exact
   activation, then exposes the already-prepared committed projection;
9. generation cancellation/target-change behavior remains compatible;
10. page load settlement exposes the new page identity and matching projection
    bank together, never a mixed old/new pair.

Required source-contract tests reject:

- `activeSynthPattern()` event-material reads from audio rendering sites;
- `SynthPattern`/`SynthStep` material copies inside AudioTask rendering paths;
- new heap-backed projection storage;
- new scheduler/queue/mutation/Undo owners;
- projection refreshes that create Scene revision, persistence or Undo effects.

Required regression verification:

```text
P0 focused characterization runner, unchanged
P1 focused host tests
host Core tests
SDL build
Cardputer ADV build
Cardputer ADV fixed-DRAM gate
SEQTRAK MIDI-only build
```

The fixed-DRAM gate must record the projection-bank memory delta explicitly.
P1 is rejected if the resident fixed-capacity design violates the accepted
Cardputer ADV internal-DRAM budget.

## Acceptance invariants

P1 is complete only when all of the following are simultaneously true:

```text
PATTERN persistent authority       Scene PATTERN only
Audio event-material authority     immutable runtime projection only
Projection material preparation    never AudioTask
Song BAR_START source switch       select prepared projection only
Generation BAR_START activation    release/select prepared state only
NO PATTERN                          canonical empty projection
Heap growth                         none
New scheduler / queue              none
New Undo owner                      none
P0 observable behavior             unchanged
```

## Hardware assumptions

P1 introduces no hardware or wiring changes. Cardputer ADV remains the target
with its existing audio configuration; SEQTRAK MIDI-only remains a build gate
only. No new I2C, GPIO, voltage, USB, DIN/UART, display or MIDI endpoint is used
by this checkpoint.
