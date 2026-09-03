# 0.9.10 PATTERN / PHRASE P1C — Runtime Foundation

## Checkpoint

`0.9.10-PATTERN-PHRASE-P1C` converges the accepted PATTERN/PHRASE research and P1 runtime-event implementation onto the current `dev_0.9.10` line.

P1C is a representation/foundation checkpoint only. It does not switch playback ownership and does not implement PHRASE.

## Base

Authoritative integration base:

```text
dev_0.9.10
2860f99d254baa96e06d48b3a52d3e729c2e707a
```

This is the merged GF2-I5 integration head used to create the P1C branch.

## Historical inputs

The old branches are evidence sources with distinct roles; they are not merged into one historical stack.

- PR #426 — P0 pre-change scheduler/lifetime characterization: regression oracle only; production delta was NONE.
- PR #427 — P1 immutable runtime synth event projection: authoritative implementation source for the value type and pure projector.
- PR #429 — P1 runtime projection design: publication/ownership constraints only, not a competing implementation lineage.

Authoritative P1C decision:

```text
#426 = regression oracle
#427 = runtime-event implementation source
#429 = publication/design constraint source
```

## Representation

The accepted API lives only in:

```text
src/phrase/runtime_synth_events.h
src/phrase/runtime_synth_events.cpp
```

Namespace: `PhraseRuntime`.

`RuntimeSynthEvent` is a fixed-size value containing:

```text
uint16_t startTick
uint16_t durationSubticks
uint8_t  note
uint8_t  velocity
uint8_t  probability
uint8_t  flags
uint8_t  fx
uint8_t  fxParam
```

Frozen constants and current ABI:

```text
kTicksPerBar                 384
kSubticksPerTick              16
kMaxPhraseBars                 8
kMaxSynthEvents              128
sizeof(RuntimeSynthEvent)      10 bytes
sizeof(RuntimeSynthEventBuffer) 1284 bytes
```

`RuntimeSynthEventBuffer` is caller-owned, trivially copyable, heap-free, deterministic, and fixed-capacity. P1C introduces no global retained runtime-event bank and does not allocate an eight-bar retained buffer merely because the value type has enough event capacity for a future eight-bar PHRASE representation.

The projector builds a local candidate buffer and publishes it to the caller only after a successful projection. Invalid synth selection leaves the destination unchanged. This is the P1C failure-atomic boundary.

## Duration contract

P1C represents duration. It does not make duration authoritative in audible playback.

```text
REPRESENTATION CAPACITY       YES
AUDIBLE COMMON LIFETIME       NO — P2
```

A projected event may encode a lifetime longer than an ordinary gate, including compatibility folding of a legacy TIE. That fact is not evidence that the production scheduler now plays arbitrary cross-bar duration.

Legacy TIE is compatibility input only. It is not promoted to the future PHRASE duration owner.

## PATTERN compatibility / P0 oracle

The P0 oracle was replayed unchanged on the current GF2-I5 base before the runtime API was introduced. It proved:

```text
Pattern physical length       16 steps
bar                            384 ticks
step                            24 ticks
GRID 8/16/32                  synth scheduler no-op
legacy TIE                    may extend an active gate across 384
swing + microtiming           modulo-wrap around the physical bar
negative microtiming          modulo-wrap around the physical bar
Song row boundary             MIDI cleanup exists
internal synth boundary       remains historically asymmetric
```

P1C projection compatibility covers:

- ordinary onset projection;
- Synth A gate scaling;
- Synth B gate scaling;
- positive timing wrap;
- negative timing wrap;
- swing compatibility;
- legacy TIE folding into explicit duration;
- expired TIE does not resurrect a note;
- a subsequent monophonic onset clips the prior projected lifetime;
- failure-atomic invalid projection.

The production scheduler is not modified to consume these values in P1C, so musician-facing PATTERN playback remains owned by the previously proven path.

## Ownership

Authoritative production ownership after P1C remains:

```text
SynthPattern
    ↓
existing production scheduler / lifetime path
    ↓
audible PATTERN playback
```

P1C adds a parallel pure value transformation API:

```text
SynthPattern
    ↓
pure immutable projection
    ↓
caller-owned RuntimeSynthEventBuffer
```

The second diagram is data preparation capacity, not a second scheduler.

P1C does not add:

- a global runtime-event bank;
- an audio-thread mutable shared event vector;
- a second Pattern owner;
- a second NoteOff owner;
- a second scheduler;
- a runtime consumer switch;
- an AudioTask projection build/copy path.

The accepted #429 publication rule remains a constraint for later work: control-originated mutations prepare immutable state outside the audio callback, and the audio side may only select already-prepared bounded state. Publication itself is not implemented by P1C because there is no runtime consumer yet.

## Source firewalls

Intended production delta is exactly the two runtime-event files above.

P1C must not modify AudioTask playback, MiniAcid scheduler ownership, Song source arbitration, PatternPlayer lifetime logic, Performance Keyboard, internal-synth Performance output, PERFORM UI, or MIDI Performance transport policy.

The focused gate enforces the production-file allow-list and explicit Performance/scheduler protected paths. PR #431 remains a parallel independent integration line; P1C resolves no future conflict on its behalf.

## Focused evidence

TDD RED on current base:

```text
59ba4a7c28bfd31b720d9e315a99f28e684087f6
```

At that SHA the full P0 source/runtime oracle passed, then the P1C build failed only because `src/phrase/runtime_synth_events.h` did not yet exist.

Minimal implementation source was then reproduced from the proven #427 projector without redesign:

```text
249c60d6065f95283b8a89dcf3b7a81c99ddcc9e
```

On that implementation SHA the focused gate proved:

```text
current dev base verified                 PASS
P0 legacy scheduler characterization     PASS
RuntimeSynthEvent fixed ABI              PASS
fixed capacity                           PASS
no heap/source contract                  PASS
failure atomic                           PASS
simple onset projection                  PASS
Synth A gate scaling                     PASS
Synth B gate scaling                     PASS
positive timing wrap                     PASS
negative timing wrap                     PASS
swing compatibility                      PASS
legacy TIE folding                       PASS
expired TIE no resurrection              PASS
next onset clips lifetime                PASS
deterministic GCC repeat                 PASS
Clang parity                             PASS
ASan                                     PASS
UBSan                                    PASS
scheduler source firewall                PASS
Performance firewall                     PASS
git diff --check                         PASS
```

The final merge candidate must re-run this gate after all documentation/test-only commits; evidence from `249c60d6...` is implementation proof, not permission to reuse a stale exact-head build result.

## Broad validation ownership

P1C does not duplicate the repository's existing target-validation owner. The authoritative exact-head broad runner is:

```text
scripts/validate_gf2_targets.sh --all
```

through `.github/workflows/gf2-e0-target-validation.yml`, which resolves and verifies the pull request head SHA before running:

```text
HOST
SDL
CARDPUTER_ADV
FIXED_DRAM
SEQTRAK_MIDI_ONLY
```

The PR remains Draft until that matrix and the focused P1C gate are terminal GREEN on the final exact P1C head. Final RAM, Flash, fixed-DRAM headroom and warning evidence belong to that exact-head PR run and must not be substituted with historical #427 numbers.

## Negative / not-yet capacity

These capabilities are explicitly NOT implemented by P1C:

```text
cross-bar physical playback       NOT YET
PHRASE source                      NOT YET
PHRASE storage/persistence         NOT YET
PATTERN xor PHRASE switching       NOT YET
MAKE PHRASE                        NOT YET
common runtime playback owner      NOT YET
GRID editor snap/zoom semantics    NOT YET
LENGTH 1/2/4/8 semantics           NOT YET
Undo changes                       NOT YET
Scene schema changes               NOT YET
MIDI lifetime redesign             NOT YET
new pattern/phrase UI              NOT YET
Performance Instrument integration NOT YET
```

## Historical PR disposition after P1C evidence

Recommended disposition only after P1C has final exact-head evidence:

- #426 — retain as historical/frozen characterization evidence;
- #427 — superseded by clean P1C integration; close only after its evidence is preserved;
- #429 — design/plan superseded by the authoritative P1C foundation and the later P2 plan.

P1C itself does not automatically close or merge any of these old PRs.

## Next boundary

After P1C, stop.

The next separate checkpoint is:

```text
0.9.10-PATTERN-PHRASE-P2
COMMON RUNTIME PLAYBACK / SINGLE LIFETIME OWNER
```

P2 must answer whether existing PATTERN playback can consume immutable runtime events through one authoritative lifetime path without changing musician-facing PATTERN behaviour.

PHRASE, 1/2/4/8-bar storage, PATTERN xor PHRASE, cross-bar duration and MAKE PHRASE remain later work and are not part of P1C.
