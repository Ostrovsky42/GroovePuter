# 0.9.10 PATTERN / PHRASE P0 — Pre-change Characterization

Status: **CHARACTERIZATION ONLY — NO PRODUCTION SEMANTIC DELTA**

## Purpose

Freeze the current PATTERN scheduler/lifetime behavior before introducing the accepted PATTERN xor PHRASE ownership model, explicit Phrase `startTick` / `durationTicks`, or any new cross-bar NoteOn/NoteOff policy.

Exact base:

```text
main
6694876edff654bc0e14cafd3181c7ff2ff5060e
```

This is the merge commit of PR #419, so the baseline already includes the single MIDI owner feeding USB + DIN/UART through `TeeMidiTransport`.

Historical v0.9.9 behavior oracle:

```text
0a2a6211ef00dcf2214dfd4704b6c34b424b1c9d
```

This document deliberately separates three categories:

1. **CURRENT OBSERVED BEHAVIOR** — what the P0 base does now;
2. **HISTORICAL UNMERGED WORK** — useful evidence that is not current ancestry;
3. **ACCEPTED FUTURE CONTRACT** — frozen design constraints for later production checkpoints, not implementation in P0.

# CURRENT OBSERVED BEHAVIOR

## GRID

`FeelSettings.gridSteps` remains persisted as 8/16/32, but `MiniAcid::processSequencerEvents()` does not consume it. Current synth playback remains the fixed 16-step scheduler:

```text
384 ticks / bar
24 ticks / physical step
16 physical steps / Pattern
```

The runtime test executes the same deterministic Synth A and Synth B Pattern under GRID 8, 16 and 32 and requires identical USB/DIN event traces.

## Exact bar boundary

The sequencer computes:

```text
barTick = absoluteTick % 384
currentStepIndex = barTick / 24
```

The test freezes:

```text
383 -> step 15
384 -> step 0
767 -> step 15
768 -> step 0
```

No off-by-one tolerance is allowed.

## Song physical-pattern transition

Current behavior is asymmetric and this is intentional characterization, not desired PHRASE semantics.

At an exact 384-tick Song row transition with `patternBars=1`:

```text
PatternPlayer MIDI ownership -> released by publishPatternAllNotesOff_()
direct internal synth voice  -> not synchronously released by applySongPositionSelection()
```

The test holds an internal gate artificially active through the transition and proves this divergence on both Synth A and Synth B. Future Phrase lifetime work must replace this with one backend-independent logical lifetime decision; P0 does not fix it.

## Legacy TIE

`SynthStep::note == -2` remains a compatibility sentinel. If an active gate reaches a TIE tick, `triggerSynthStep_()` extends the existing gate countdown. P0 demonstrates the reachable cross-boundary symptom:

```text
step15 onset timing +23 -> barTick 383
physical boundary       -> 384
step0 TIE timing +1     -> barTick 1 / absolute tick 385
```

The gate is extended, while no new MIDI NoteOn is emitted.

This is **legacy compatibility behavior**, not authoritative Phrase lifetime representation. Future PHRASE data must not encode duration by promoting the `-2` sentinel.

## Swing / microtiming modulo wrap

Current scheduler uses modulo-384 trigger coordinates.

With maximum 75% swing on odd step 15:

```text
360 + 12 = 372
```

so swing alone remains inside the bar.

With step15 microtiming `+23` as well:

```text
(360 + 12 + 23) % 384 = 11
```

so the old physical step is observed at barTick 11.

A step0 microtiming of `-23` is observed at:

```text
(0 - 23 + 384) % 384 = 361
```

These are scheduler-position wraps. They do not establish phrase-wide note ownership or designed cross-bar note lifetime.

## Synth A / Synth B and USB / DIN

Every lifetime/timing case is executed for both synth tracks. MIDI acceptance uses one real `UsbMidiOutput` over the real `TeeMidiTransport` with accepting fake USB and DIN transports. The two endpoint traces must be byte-identical for every characterized case.

No second MIDI lifetime owner is authorized.

## M2 carrier is present but intentionally all-false

The current source contains the fixed-capacity `MelodicCrossBarLifetime` semantic carrier, but current production phrase execution does not produce non-trivial values. `src/generation/migration/phrase_execution.cpp` explicitly states:

```text
P1R deliberately has no non-trivial cross-bar lifetime producer yet.
The fixed-capacity carrier remains present and all-false.
```

and assigns `MelodicCrossBarLifetime{}` for each materialized bar.

Therefore current `false / false` is **not** a feature flag, accidental default, or unexplained regression. It is the intentionally landed P1R state without C2 production and without physical R1 execution.

Current status:

```text
M2 REPRESENTATION            IMPLEMENTED
M2 BOUNDARY DECISION MODEL   IMPLEMENTED
NON-TRIVIAL PRODUCER
IN CURRENT MAIN              NOT IMPLEMENTED
PHYSICAL RUNTIME LIFETIME
USB/DIN/INTERNAL SYNTH       NOT IMPLEMENTED
CURRENT all-false            INTENTIONAL P1R STATE
```

## Undo and transaction ownership

PR #340 is the authoritative R8/R9 history contract:

```text
Ctrl+Z              public history gesture
Ctrl+U              not an alias
retained history    one fixed before/after pair
new mutation        republishes slot and resets direction to UNDO
canonical owner     GroovePuterUndo::UndoOwner
```

Phrase work must not add a second Undo/Redo owner or stack.

The generated-Phrase-to-Song path is already explicitly distinct from an ordinary manual Song arrangement edit. `GeneratedPhraseSong::GeneratedPhraseUndoPayload` is committed as `UndoKind::Generation`, not `UndoKind::Song`. Its current rollback state includes the pre-generation `Song`, target page/slot/range coordinates, generated bar count, and previous `feel.patternBars`; rollback validates the exact generated range, clears the generated physical Synth A/Synth B/Drum slots, restores `beforeSong`, and restores `previousPatternBars`.

Future authoritative PHRASE material changes this atomic rollback boundary. If a Song slot points to PHRASE-owned material, the generation transaction must restore, as one logical operation:

- the Song arrangement/reference state;
- the authoritative PHRASE object state that was created/replaced/referenced by that transaction;
- any source-ownership metadata needed to make those references valid again;
- the existing generation-specific state already covered by the authoritative Generation receipt.

That state belongs in the existing canonical Undo/Generation receipt model. It must not be silently reclassified as a manual Song edit, and P0 does not implement the new payload.

## Scene/project persistence owner

The current Scene JSON root is not globally schema-versioned. The existing phrase-related persisted sub-format is `PhraseCore`:

```text
src/phrase/phrase_types.h
PhraseCore::kPersistenceVersion = 1
PhraseBank::version = kPersistenceVersion
```

`SceneJsonEmitter` writes `scene.phraseBank` as the `phraseCore` array, whose first persisted value is `PhraseBank::version`; `PhraseCore::sanitize()` resets the existing PhraseCore bank when that sub-format version does not match.

This existing `PhraseCore` stores phrase metadata plus Pattern references. It is **not** the accepted future synth PHRASE source with explicit `startTick` / `durationTicks` notes, and its version must not be mistaken for a project-wide compatibility gate.

For future PATTERN/PHRASE persistence, the authoritative hard-version gate therefore belongs at the Scene/project JSON owner — the `SceneJsonEmitter` / `SceneJsonObserver` serialization boundary in `scenes.h` / `scenes.cpp` — before a newer Phrase-bearing document can be interpreted by an older layout. The later persistence checkpoint must add an explicit root Scene/project format version there and reject unsupported newer versions before applying state.

P0 does **not** perform that bump and adds no placeholder fields.

Compatibility contract for that later bump:

```text
old PATTERN-only project -> new firmware: LOAD
new Phrase-bearing format -> old firmware: FAIL CLOSED
new Phrase-bearing format -> new firmware: LOAD
```

Silently ignoring/truncating PHRASE data into a one-bar PATTERN is forbidden.

## Current Synth A/B Tab and keymap

The current parent-owned Synth tab cycle is exactly:

```text
[N]KM  NOTES
N[K]M  KNOBS
NK[M]  MORE
```

Plain `Tab` cycles `NOTES -> KNOBS -> MORE -> NOTES`; modified Tab is not consumed by this local cycle. Documentation must not use the obsolete `NOTES / SOUND / MORE` wording.

`Tab` is therefore unavailable for PATTERN/PHRASE ownership switching.

`V` is also not acceptable as a bare source switch because `Ctrl+V` is part of the editing/reset vocabulary and historical Phrase UI used bare V for unrelated view behavior.

The initially proposed `Alt+V` also conflicts: the current global keymap owns `Alt+V` as the hard-global GENERATION/GENRE compatibility navigation action. It is rejected for PATTERN/PHRASE switching.

The P0 keymap audit found no global or Synth NOTES owner for `Alt+Y`: it is absent from the authoritative keymap matrix and current source search found no `GROOVEPUTER_Y` / `lowerKey == 'y'` handler. Therefore the documented future candidate is:

```text
Alt+Y -> PATTERN / PHRASE source view/action
```

This is a documentation reservation only. A later UI checkpoint must still implement it visibly and retain the collision guard. No bare single-letter switch, Paste collision, Tab stealing, or hidden source-mode switch is allowed.

# HISTORICAL UNMERGED WORK

## M2 representation and boundary decision model

Commit:

```text
bedc0465cf9e9e847cebae8910ead3798e7c3f33
feat(0.9.9-m2): add bounded cross-bar lifetime contract
```

introduced the backend-neutral semantic carrier:

```cpp
struct MelodicCrossBarLifetime {
  bool entersFromPreviousBar = false;
  bool continuesIntoNextBar = false;
};
```

and the boundary decision rule:

```text
IntraPhraseBarAdvance + continuesIntoNextBar -> Continue
Stop                                         -> Release
OutsideLogicalPhrase                         -> Release
```

M2-T1 implemented representation and the logical decision model only. It did not implement physical playback, MIDI dispatch, internal synth gate execution, or a producer of non-trivial lifetime values.

P1R later wired that carrier into production execution while deliberately keeping it all-false; its contract explicitly excluded C2.

## C2-C0 topology characterization

Historical PR #395 (`0.9.9 PHRASE-C2-C0`) characterized natural production topology and reached:

```text
DECISION A
NATURAL PURE-MELODIC CROSSBAR TOPOLOGY REACHABLE

A_ONSET         REACHABLE
A_CONTINUATION  UNREACHABLE
A_OVERLAP       ZERO
```

This authorized investigation of **A_ONSET only**. It did not authorize generalized continuation or hybrid lifetime semantics. Physical-gate evidence in that research remained characterization only; future R1 was still the physical execution owner.

## C2 candidate was never merged

Historical PR #396:

```text
feat(0.9.9-phrase): produce minimal A-onset cross-bar lifetime
head: 2f9b6c7659bb4e0560c129ee33951f7adfcba8a4
state: CLOSED
merged: FALSE
```

implemented a candidate semantic producer limited to pure-melodic A_ONSET, ordinary sequential intra-phrase boundaries, paired outgoing/incoming flags, and fail-closed behavior elsewhere.

The PR explicitly excluded physical gate changes, runtime sustain, transport/MIDI/internal-synth lifecycle changes, and stated `R1 is NOT started`.

Therefore #396 is historical evidence, not implementation ancestry for P0. It must not be cherry-picked into this branch.

The correct historical chain is:

```text
M2 semantic carrier
  -> P1R wires carrier intentionally all-false
  -> C2-C0 proves natural A_ONSET topology
  -> C2 candidate produces minimal A_ONSET flags
  -> candidate closes unmerged
  -> physical R1 never lands
  -> current main remains intentionally all-false
```

This is not described as a rollback because the known C2/R1 work was never merged and later reverted.

# ACCEPTED FUTURE CONTRACT

The detailed target architecture is frozen in:

```text
docs/contracts/0_9_10_PATTERN_PHRASE_ARCHITECTURE.md
```

The checkpoint sequence is frozen in:

```text
docs/contracts/0_9_10_PATTERN_PHRASE_NEXT_CHECKPOINTS.md
```

P0 records, but does not implement, these constraints:

- one authoritative musical source at a time: PATTERN xor PHRASE;
- PATTERN remains the existing one-bar 16-step source;
- PHRASE uses explicit `startTick` / `durationTicks` and length 1/2/4/8 bars;
- playback consumes an immutable runtime event projection, never a third musical owner;
- preparation stays outside the audio callback and publication uses `PREPARE -> COMMIT -> AudioMutationGate`;
- `MAKE PHRASE` is explicit and one-way; no reverse synchronization;
- future GRID is cursor/snap/zoom only: 1/8=48, 1/16=24, 1/32=12 ticks per bar division;
- GRID never moves or reinterprets existing note times;
- LENGTH is independent of GRID, FEEL CYCLE, Pattern page and Song row count;
- one canonical Undo owner remains authoritative;
- Phrase-bearing project persistence requires a hard Scene/project format bump and fail-closed old-firmware behavior;
- source UI must be visible and use an explicit non-colliding modified action; the P0 candidate is `Alt+Y`.

## Hardware list

Host characterization:

- Linux runner;
- GCC;
- Clang when available;
- ASan;
- UBSan;
- SDL2 / SDL2_gfx host build dependencies.

No Cardputer ADV or SAM2695 hardware is required for P0. Hardware lifetime acceptance belongs to a later PR after production Phrase lifetime exists.

## Wiring

None for P0.

The merged PR #419 DIN-UART wiring/configuration is not changed by this checkpoint.

## Build / validation

Run:

```sh
bash tests/run_pattern_phrase_p0_characterization.sh
```

The runner:

1. verifies the exact base commit exists;
2. requires zero `src/` delta from the base;
3. runs source ownership checks;
4. builds/runs the real SDL MiniAcid runtime harness with GCC twice;
5. requires deterministic repeat;
6. checks Clang parity when available;
7. runs ASan;
8. runs UBSan;
9. repeats the production `src/` firewall.

## Expected behavior

Successful output includes:

```text
P0 source contract: OK
P0-A PASS: GRID 8/16/32 is a synth scheduler no-op
P0-B PASS: physical bar boundary is exactly 384 ticks
P0-C PASS: Song row @384 cleans MIDI while direct internal gate survives
P0-D PASS: legacy TIE can extend an already-active gate across 384
P0-E PASS: swing alone reaches 372; swing+micro wraps step15 to tick11
P0-F PASS: step0 microtiming -23 wraps to barTick361
PATTERN/PHRASE P0 runtime characterization: OK
PATTERN/PHRASE P0 characterization: PASS
```

## Troubleshooting

### GRID traces differ

Stop. Do not normalize the test. Find the current consumer that makes `gridSteps` alter synth playback and revise the architecture before Phrase implementation.

### Song boundary releases internal voice too

Stop. That means current post-#419 behavior differs from the frozen P0 characterization. Record the new exact behavior instead of forcing the old expectation.

### USB and DIN traces differ with both fake endpoints accepting

Stop. Investigate the already-merged tee/output ownership path before Phrase lifetime. Do not add a second MIDI owner.

### TIE does not extend the gate

Verify the active gate reaches the TIE tick. P0 does not authorize changing `-2`; it only freezes the existing compatibility behavior.

### Sanitizer output differs

Any ASan/UBSan diagnostic is a failure. Do not suppress a production bug to make characterization green.

## Acceptance checklist

- [ ] exact base is `6694876edff654bc0e14cafd3181c7ff2ff5060e`
- [ ] production `src/` delta is zero
- [ ] GRID 8/16/32 scheduler trace invariant on Synth A
- [ ] GRID 8/16/32 scheduler trace invariant on Synth B
- [ ] exact 384-tick boundary frozen
- [ ] Song-boundary MIDI/internal divergence frozen on Synth A
- [ ] Song-boundary MIDI/internal divergence frozen on Synth B
- [ ] legacy TIE crossing classified as compatibility only
- [ ] swing-only 372 coordinate frozen
- [ ] swing + microtiming modulo wrap to tick 11 frozen
- [ ] negative step0 microtiming wrap to tick 361 frozen
- [ ] USB/DIN accepting-endpoint traces identical
- [ ] M2 representation/decision ancestry documented
- [ ] current all-false state explained as intentional P1R without C2/R1
- [ ] C2-C0 A_ONSET-only topology result documented
- [ ] PR #396 recorded as closed/unmerged historical candidate
- [ ] physical R1 recorded as never landed
- [ ] PATTERN xor PHRASE target architecture frozen separately
- [ ] one canonical Undo owner frozen; Generation receipt remains distinct from Song edits
- [ ] PHRASE-aware atomic rollback requirements documented
- [ ] current Scene/PhraseCore persistence-version ownership audited
- [ ] future hard Scene/project root-format bump requirement documented
- [ ] NOTES / KNOBS / MORE Tab cycle verified
- [ ] `Alt+V` conflict documented and `Alt+Y` future action collision-audited
- [ ] immediate next checkpoint is immutable runtime event projection only
- [ ] deterministic GCC repeat PASS
- [ ] Clang parity PASS when available
- [ ] ASan PASS
- [ ] UBSan PASS
- [ ] no production implementation in this PR

## Decision after GREEN

```text
CURRENT PATTERN BEHAVIOR CHARACTERIZED  YES
M2 HISTORY / ALL-FALSE EXPLAINED       YES
TARGET ARCHITECTURE FROZEN IN DOCS     YES
PRODUCTION SEMANTIC DELTA              NONE
PHRASE LIFETIME IMPLEMENTATION         NOT STARTED
NEXT                                   IMMUTABLE EVENT PROJECTION CONTRACT
```
