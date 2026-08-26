# 0.9.9-M2-A1 — Cross-bar note lifetime contract audit

Status: **CLOSED — EXECUTABLE CHARACTERIZATION COMPLETE**

Frozen base: `5ad44bb9400ea38d349b7f815f84f833fb18ce6a`

Executable pre-closure head: `c7dcc489a2904e4de23f57fa56f918ac790ebdb0`

Branch: `research/20260826-07-0.9.9-m2-a1-crossbar-note-lifetime`

Scope: research / characterization only. No production note-lifetime changes.

## Purpose

M1 established one stable logical melodic phrase across multiple physical Synth bars. M2 asks which representation and runtime owner can keep one already-started note alive across a physical pattern boundary.

## Current lifecycle

```text
MelodicMotifPlan
  onsets + bar-local continuations
        |
        v
TonalMaterializer / tonal_pattern_adapter
        |
        v
SynthPattern / SynthStep
        |
        v
MiniAcid PatternPlayer
  triggerSynthStep_()
  gateCountdownA_/B_
  direct internal synth voice
        |
        +----------------------+
        |                      |
        v                      v
 internal voice          MusicalEventQueue
                                |
                                v
                         MidiDispatchTask
                                |
                                v
                          UsbMidiOutput
```

`SynthStep` currently contains note, slide, accent, ghost, velocity, timing, fx, fxParam and probability. It has no authoritative semantic duration/tie/hold/cross-bar-lifetime field or contract.

## Representation capability

### Semantic continuation

Current melodic continuation topology is bar-local. Validation starts each semantic plan without an already-active note, therefore a continuation at the beginning of a new physical bar is not a legal way to refer to a note started in the previous bar.

The tonal adapter materializes a valid local continuation by copying the current pitched `SynthStep` and setting `slide=true`. This remains another pitched physical step and therefore another PatternPlayer onset; it is not a phrase-wide lifetime token.

### Legacy `note == -2` sentinel

MiniAcid contains a legacy playback `note == -2 // TIE` sentinel. That sentinel is a real frozen-base runtime fact, but it is **not** the authoritative production generation representation for cross-bar note lifetime:

- there is no corresponding explicit current melodic semantic lifetime contract;
- `tonal_pattern_adapter` does not emit `note == -2` as the normal semantic lifetime representation;
- playback accepts the sentinel only while an existing gate countdown remains positive;
- it has no independently specified cross-pattern ownership or MIDI/internal parity contract.

The initial executable source guard incorrectly also required obsolete historical storage:

```cpp
std::array<uint8_t, 16> tie{};
```

inside `AdvancedPatternGenerator`. That storage is not present on the frozen base and is not part of the M2 ownership contract. The requirement was removed test-only. The audit does not search for, restore, or depend on that historical storage.

Therefore the presence of the MiniAcid `note == -2` sentinel does **not** close the phrase-wide note-lifetime representation gap.

Future M2 work must not adopt or extend `-2` as the authoritative cross-bar representation merely because that compatibility branch exists. Legacy behavior may remain untouched until explicitly migrated.

## Executed boundary characterization

`tests/test_m2_crossbar_note_lifetime_contract.py` is the deterministic source characterization guard.

`tests/test_m2_crossbar_note_lifetime_runtime.cpp` is the host runtime characterization. It links the real SDL MiniAcid implementation, uses the existing `MusicalEventQueue` and `UsbMidiOutput`, and opens private engine state only inside the research translation unit for observation. It does not add a production probe or seam.

The runner is:

```bash
bash tests/run_m2_a1_crossbar_note_lifetime.sh
```

On exact executable head `c7dcc489a2904e4de23f57fa56f918ac790ebdb0`, focused CI completed successfully twice:

- run `32993788981` / M2-A1 run #12 — **SUCCESS**
- run `32994600809` / M2-A1 run #14 — **SUCCESS**

The same code line also has successful normal Core regression executions including:

- `32993788527` — **SUCCESS**
- `32994600894` — **SUCCESS**

Frozen execution evidence:

| Evidence | Result |
| --- | --- |
| source characterization | PASS |
| GCC runtime A-H | PASS |
| GCC deterministic repeat | PASS |
| Clang parity | PASS |
| ASan / UBSan | PASS |
| independent focused rerun | PASS |
| normal Core regression | PASS |

No claim beyond the existing runner/logs is implied by this table.

## Frozen current lifetime characterization

Normal bar-local materialization does not provide authoritative phrase-wide lifetime state.

The executed A-H characterization freezes these lifecycle boundaries for the current code line:

- **Stop** is a hard lifetime barrier.
- **Explicit pattern switch** is currently a hard lifetime boundary.
- **Song physical-pattern transition** is currently a hard lifetime boundary.

These current boundaries do not grant permission for future internal-synth and MIDI lifetime semantics to diverge. M2-T1 must define backend-independent logical lifetime/barrier semantics first; internal synth and MIDI output must then observe the same authoritative lifetime decision.

## Internal synth vs MIDI ownership

PatternPlayer internal audio is rendered directly by MiniAcid. `InternalSynthOutput` deliberately ignores `MusicalEventSource::PatternPlayer` fan-out to avoid creating a second internal trigger owner.

PatternPlayer MIDI uses the event queue and `UsbMidiOutput`. Song/pattern lifecycle publication is a separate execution mechanism from the direct internal voice path.

M2 must not "fix" parity by simply routing PatternPlayer events back through `InternalSynthOutput`. The authoritative lifetime decision belongs at sequencer / PatternPlayer playback ownership while internal audio and MIDI remain separate execution mechanisms.

Forward M2-T1 requirement:

```text
one logical lifetime decision
        |
        +--> internal synth execution
        |
        +--> MIDI execution

CONTINUE / RELEASE / RETRIGGER
must mean the same musical decision on both backends.
```

Any backend disagreement is a contract failure even if neither side leaves an orphan note.

## Frozen M2/M3 ownership boundary

```text
M2 owns:
  whether an already-started note remains alive

M3 owns:
  which harmonic source is active at phrase time

M3 does NOT:
  retrigger or retune a held note merely because harmony changed

M2 does NOT:
  advance or select harmonic source
```

Pitch/harmonic projection is decided at note onset. If harmony changes while M2 keeps a note alive, that already-started note remains alive without an M3-forced NoteOn or retune. The next new melodic onset uses the then-current M3 harmonic source.

Future joint M2+M3 integration acceptance should include:

```text
bar0 onset under harmony A
harmony changes to B while note is held
held note continues unchanged
next new melodic onset uses harmony B
```

## Stop invariant

Stop is the negative lifetime invariant for future M2 work:

```text
After Stop:
  internal active lifetime = none
  gate countdown = 0
  MIDI PatternPlayer ownership = none
  pending cross-bar carry = none
```

M2-T1/P1 must preserve this as a regression guard.

## Owner decision

The future cross-bar lifetime **decision owner should be the sequencer / PatternPlayer playback lifecycle**, because that layer already owns gate countdown, physical pattern transitions, direct internal voice execution and PatternPlayer event publication.

Semantic/tonal layers must provide enough bounded representation to request a hold, but they must not independently own output lifetime. MIDI dispatcher remains an executor/cleanup owner, not the musical lifetime policy owner.

## Decision

**DECISION B — CROSS-BAR NOTE LIFETIME REPRESENTATION GAP**

This decision is now frozen by executable characterization.

The current semantic + `SynthStep` path cannot express an authoritative genuine phrase-wide cross-bar hold. The existence of the legacy MiniAcid `note == -2` sentinel does not supply that missing production representation, and `tonal_pattern_adapter` does not emit the sentinel as the normal semantic lifetime representation.

This is not Decision C: playback ownership/barrier behavior matters, but even a perfect playback owner would still lack a current authoritative semantic/physical representation for an explicit phrase-wide lifetime request.

## Production boundary

Production `src/` semantic delta from frozen M1 base is **ZERO**.

No changes were made by M2-A1 to:

- MiniAcid;
- SynthStep;
- tonal_pattern_adapter;
- PatternPlayer runtime;
- MIDI runtime;
- generation semantics.

## Next checkpoint

M2-T1 is **NOT STARTED**.

M2-P1 is **NOT STARTED**.

A future M2-T1 may specify the smallest bounded explicit lifetime representation capable of distinguishing onset, continuation/hold and release across physical bars while preserving:

- sequencer as the single lifetime decision owner;
- separate internal and MIDI execution mechanisms with backend-independent lifetime decisions;
- Stop as a hard barrier;
- reviewed pattern/Song transition barrier semantics;
- legacy `note == -2` compatibility without making it authoritative;
- no harmonic-source ownership in M2.

No M2 implementation is part of this audit.
