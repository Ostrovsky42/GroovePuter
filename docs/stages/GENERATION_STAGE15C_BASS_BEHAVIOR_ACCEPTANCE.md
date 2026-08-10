# Generation Stage 15C — Bass Pitch Behavior + Articulation

## Purpose

Stage 15C adds bounded, deterministic one-bar bass pitch behavior and
engine-neutral articulation intent for Synth A. It consumes an already-resolved
bass rhythm topology and assigns scale-degree movement, accent intent, and
slide-into intent without changing where bass onsets or continuations occur.

Current reachability: **API-ONLY / host-testable**. Production migration wiring is
intentionally deferred until the moving Stage 14 materialization contract is
stabilized. Stage 15C does not claim Stage 9 rhythm ownership and does not claim
multi-bar bass evolution.

## Hardware list

- M5Stack Cardputer-Adv.
- USB-C cable for build/flash and serial.
- Built-in speaker is sufficient after production wiring.
- Optional Yamaha SEQTRAK for later MIDI audition.

## Wiring

No new wiring is introduced by Stage 15C.

- Cardputer-Adv is powered/programmed over USB-C.
- No new GPIO, I2C, SPI, or UART ownership.
- Existing PORT.A I2C invariants are unchanged and unused by this stage.

## Build / Flash

Host gate for this isolated checkpoint:

```bash
bash tests/run_generation_stage15c_tests.sh
```

The gate compiles and executes with GCC, Clang when available, and ASan/UBSan.

There is no Stage 15C-specific firmware flash acceptance yet because this branch
has no production caller wiring. After Stage 14 stabilizes, the integration
branch must pass the normal Cardputer-Adv and SEQTRAK MIDI-only builds before
hardware audition.

## Expected behavior

Host tests must show:

```text
Generation Stage 15C source regressions: OK
Generation Stage 15C host matrix: OK
```

The API must:

- preserve the input bass onset and continuation masks exactly;
- produce deterministic scale-degree offsets for identical inputs;
- support RootAnchor, RootFifth, RootOctave, NeighborReturn, StepApproach,
  LeapReturn, RootFifthNeighbor, and PedalTurn contours;
- keep pitch movement inside the requested degree and maximum-leap bounds;
- emit accent and slide-into masks that are strict subsets of real bass onsets;
- never create a slide target on the first onset;
- return a valid empty result for a valid empty bass rhythm plan;
- use fixed-capacity storage with no heap allocation or global RNG;
- remain independent from Synth B, Phrase, Scene, and physical synth type.

## Troubleshooting

If compilation fails in the Stage 15C test:

1. Confirm the branch is based on the current Stage 14 generation headers.
2. Confirm `src/generation/generation_context.cpp` is included in the host build.
3. Do not move bass onset generation from `bass_rhythm.*` into Stage 15C.
4. Do not map semantic articulation directly to TB303/SID/AY/other engine
   internals in this API-only stage.
5. If Stage 14 changes `BassRhythmPlan`, rebase this isolated branch and adapt
   only the input adapter.

If the source regression reports a forbidden owner such as `Scene`, `PhraseCore`,
`StrongRhythmMigration`, `SynthPattern`, melodic code, heap containers, or global
randomness, the implementation has crossed the Stage 15C architecture boundary.

## Acceptance checklist

- [ ] `bash tests/run_generation_stage15c_tests.sh` passes with GCC.
- [ ] Clang run passes when Clang is installed.
- [ ] ASan/UBSan run passes.
- [ ] Input bass onset mask equals output onset mask.
- [ ] Input continuation mask equals output continuation mask.
- [ ] Identical initial state produces identical bass behavior.
- [ ] Auto selection reaches more than one legal contour/articulation behavior
      over the deterministic test matrix.
- [ ] Degree and maximum-leap bounds are never violated.
- [ ] Accent and slide masks never contain a non-onset cell.
- [ ] No new onset or continuation is synthesized.
- [ ] No heap allocation or global RNG is introduced.
- [ ] No Scene, Phrase, Synth B, voice-allocation, or synth-engine ownership
      appears.
- [ ] Production reachability remains explicitly unclaimed until Stage 14 wiring
      is stabilized and separately reviewed.
- [ ] Hardware bass musical verdict remains **PENDING**.
