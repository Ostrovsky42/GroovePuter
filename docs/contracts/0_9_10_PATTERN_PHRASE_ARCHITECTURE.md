# 0.9.10 PATTERN / PHRASE — Architecture Contract

Status: **FROZEN TARGET CONTRACT — NOT IMPLEMENTED BY P0**

This document records the accepted PATTERN/PHRASE architecture that later production checkpoints must implement incrementally. It does not authorize a framework rewrite, a second playback path, or a second persistence/history owner.

## 1. Authoritative musical source: PATTERN xor PHRASE

For a Synth fragment, exactly one musical source is authoritative:

```text
PATTERN xor PHRASE
```

Never both.

### PATTERN

PATTERN remains the current one-bar `SynthPattern` model:

```text
16 physical steps
384 ticks / bar
24 ticks / physical step
```

Existing PATTERN-only projects must continue to load without conversion. Current PATTERN semantics remain unchanged unless a later dedicated checkpoint explicitly changes them.

### PHRASE

PHRASE is a separate multi-bar musical source with an authoritative length of:

```text
1 / 2 / 4 / 8 bars
```

Its notes use explicit musical time:

```text
startTick
durationTicks
```

Independent one-bar `SynthPattern` objects are not the authoritative representation of PHRASE note lifetime.

The existing `PhraseCore::PhraseBank` is not this new source: it currently stores phrase metadata and Pattern references. Do not silently reinterpret that representation as explicit note-time PHRASE storage.

## 2. Immutable runtime event projection

Playback consumes a read-only runtime event projection of whichever source is authoritative:

```text
PATTERN -----\
             \
              -> immutable runtime events -> scheduler/backends
             /
PHRASE ------/
```

The projection is derived state only. It is **not** persistence, editing storage, an Undo owner, or a third musical source.

The immediate production checkpoint after P0 owns only this projection contract.

## 3. PREPARE / COMMIT / AudioMutationGate

Mutable musical planning and projection preparation occur outside the audio callback.

Publication follows the existing atomic mutation architecture:

```text
PREPARE
  ↓
COMMIT
  ↓
AudioMutationGate
```

The audio callback consumes already-prepared immutable state. Heap allocation, mutable musical planning, persistence edits, Undo construction, and Pattern/Phrase conversion are forbidden inside the audio callback.

Do not create a parallel publication mechanism for PHRASE.

## 4. MAKE PHRASE ownership transfer

Conversion is explicit and one-directional:

```text
PATTERN
  ↓
MAKE PHRASE
  ↓
PHRASE
```

After conversion, Pattern and Phrase are separate objects. There is no automatic reverse synchronization and no continuously mirrored authoritative copy.

A later owner/persistence checkpoint must define the exact persistent mutation and canonical Undo payload for this transfer. P0 does not implement it.

## 5. GRID semantics

Future GRID is an editing/view axis only:

- cursor step;
- snap for newly created or moved material;
- editing zoom/window granularity.

At 384 ticks per bar:

```text
1/8  = 48 ticks
1/16 = 24 ticks
1/32 = 12 ticks
```

Changing GRID must not change tempo, move existing notes, reinterpret existing `startTick` values, or reinterpret existing `durationTicks` values.

The current P0 fact that GRID 8/16/32 is ignored by synth playback is a pre-change baseline, not the future implementation of this contract.

## 6. LENGTH semantics

PHRASE LENGTH is an independent authoritative axis:

```text
1 / 2 / 4 / 8 bars
```

It must not be owned by or inferred from GRID, FEEL CYCLE / `FeelSettings.patternBars`, the physical Pattern page, or Song row count.

## 7. Lifetime and backend ownership

Current `MelodicCrossBarLifetime` already provides a backend-neutral semantic boundary decision shape, but current P1R production values are intentionally all-false and no physical R1 execution exists.

Future physical lifetime execution must consume one authoritative duration/lifetime decision and apply it consistently to:

```text
internal synth
USB MIDI
DIN/UART MIDI
```

There must not be separate musical lifetime policies per backend and there must not be a second MIDI lifetime owner beside the existing logical MIDI output / `TeeMidiTransport` topology.

Legacy `SynthStep::note == -2` remains compatibility behavior and is not the PHRASE duration representation.

## 8. Undo / Redo ownership

The R8/R9 contract from PR #340 remains authoritative:

- `Ctrl+Z` is the public history gesture;
- `Ctrl+U` is not an alias;
- one canonical `GroovePuterUndo::UndoOwner` owns one fixed retained before/after pair;
- a newly accepted persistent mutation republishes that slot and resets direction to UNDO.

PHRASE must not introduce `PhraseUndoOwner`, `PhraseUndoStack`, or a second Redo owner. Future Phrase editing is a new payload kind handled by the canonical owner.

Generation remains semantically distinct from an ordinary manual Song arrangement edit. If generated publication creates or replaces PHRASE-owned material referenced by Song, its rollback receipt must restore the Song references and the authoritative PHRASE state atomically, together with the generation-specific state already owned by that transaction.

## 9. Scene/project persistence compatibility

Current relevant versioned phrase persistence is `PhraseCore::kPersistenceVersion = 1`, serialized inside the `phraseCore` Scene JSON field. The Scene JSON root itself is not currently a hard globally-versioned format.

The future explicit-note PHRASE source changes that compatibility requirement. A project containing PHRASE data must carry a hard Scene/project format version owned by the Scene serialization boundary (`SceneJsonEmitter` / `SceneJsonObserver` in `scenes.h` / `scenes.cpp`).

Required compatibility behavior:

```text
old PATTERN-only project -> new firmware: LOAD
new Phrase-bearing format -> new firmware: LOAD
new Phrase-bearing format -> old firmware: FAIL CLOSED
```

Old firmware must not silently ignore the new source or reinterpret it as a truncated one-bar PATTERN.

The version bump and fields belong to the later authoritative PHRASE source/persistence checkpoint, not P0 and not the immutable projection checkpoint.

## 10. UI ownership

Current Synth A/B parent tab ownership remains:

```text
[N]KM  NOTES
N[K]M  KNOBS
NK[M]  MORE
```

Plain Tab remains the local three-state cycle and must not be overloaded for PATTERN/PHRASE switching.

No bare single-letter source switch is allowed. `Alt+V` is already hard-global GENERATION/GENRE navigation and is unavailable. P0 therefore reserves the collision-audited future candidate:

```text
Alt+Y -> PATTERN / PHRASE source view/action
```

The later UI checkpoint must expose source state visibly to the musician; switching must not be a hidden mode.

## 11. Non-goals of this architecture contract

This document does not itself implement persistent PHRASE note storage, event projection code, MAKE PHRASE, cross-bar sustain, C2 producer resurrection, MIDI/internal-synth lifecycle changes, GRID scheduler behavior, Scene format bump, Phrase Undo payload, or UI switching.

Each is gated by a later single-contract checkpoint.

## Acceptance checklist

- [ ] exactly one authoritative source: PATTERN xor PHRASE
- [ ] PATTERN remains current 16-step one-bar source
- [ ] PHRASE length is independently 1/2/4/8 bars
- [ ] PHRASE note time is explicit `startTick` + `durationTicks`
- [ ] runtime events are immutable projection, not a third owner
- [ ] projection preparation remains outside audio callback
- [ ] publication reuses `PREPARE -> COMMIT -> AudioMutationGate`
- [ ] MAKE PHRASE is explicit and one-way
- [ ] GRID is cursor/snap/zoom only and never retimes existing notes
- [ ] LENGTH is independent of GRID/FEEL/Pattern/Song row count
- [ ] one canonical Undo owner remains authoritative
- [ ] generated PHRASE rollback remains a Generation transaction
- [ ] Phrase-bearing persistence requires a hard Scene/project format gate
- [ ] old PATTERN-only projects remain loadable
- [ ] Tab remains NOTES/KNOBS/MORE
- [ ] future source action is explicit, modified and visibly exposed
