# Generation Stage 15B — Melodic Intent

## Purpose

Stage 15B adds bounded, deterministic one-bar melodic pitch intent for Synth B.
It consumes the already-resolved Stage 14 melodic rhythm topology and assigns a
scale-degree contour plus a local motif operation. It does not own physical
Synth B arbitration, chord blocking, full-groove diversity, Phrase state, bass
behavior, or multi-bar evolution.

Current reachability: **API-ONLY / host-testable**. Production migration wiring is
intentionally deferred until the moving Stage 14 materialization contract is
stabilized. Do not claim hardware musical acceptance from this checkpoint.

## Hardware list

- M5Stack Cardputer-Adv.
- USB-C cable for build/flash and serial.
- Built-in speaker is sufficient after production wiring.
- Optional Yamaha SEQTRAK for later MIDI audition.

## Wiring

No new wiring is introduced by Stage 15B.

- Cardputer-Adv is powered/programmed over USB-C.
- No new GPIO, I2C, SPI, or UART ownership.
- Existing PORT.A I2C invariants are unchanged and unused by this stage.

## Build / Flash

Host gate for this isolated checkpoint:

```bash
bash tests/run_generation_stage15b_tests.sh
```

The gate compiles and executes with GCC, Clang when available, and ASan/UBSan.

There is no Stage 15B-specific firmware flash acceptance yet because this branch
has no production caller wiring. After Stage 14 stabilizes, the production
integration branch must also pass the normal Cardputer-Adv build before hardware
audition.

## Expected behavior

Host tests must show:

```text
Generation Stage 15B source regressions: OK
Generation Stage 15B host matrix: OK
```

The API must:

- preserve the input melodic onset and continuation masks exactly;
- produce deterministic scale-degree offsets for identical inputs;
- support Static, StepUp, StepDown, Arch, InvertedArch, LeapReturn, Neighbor,
  RepeatThenUp, and RepeatThenDown contours;
- apply only one-bar pitch-domain motif operations that preserve onset count;
- keep every degree inside the requested bounds and adjacent movement inside the
  requested maximum leap;
- return a valid empty result for a valid empty melodic rhythm plan;
- use fixed-capacity storage with no heap allocation or global RNG.

## Troubleshooting

If compilation fails in the Stage 15B test:

1. Confirm the branch is based on the current Stage 14 generation headers.
2. Confirm `src/generation/generation_context.cpp` is included in the host build.
3. Do not repair Stage 14 hybrid chord/melody arbitration inside Stage 15B.
4. If Stage 14 changes `MelodicMotifPlan`, rebase this isolated branch and adapt
   only the input adapter; do not add Phrase or voice-allocation ownership.

If the source regression reports a forbidden owner such as `Scene`, `PhraseCore`,
full-groove fingerprint history, heap containers, or `StrongRhythmMigration`,
the implementation has crossed the Stage 15B architecture boundary.

## Acceptance checklist

- [ ] `bash tests/run_generation_stage15b_tests.sh` passes with GCC.
- [ ] Clang run passes when Clang is installed.
- [ ] ASan/UBSan run passes.
- [ ] Input onset mask equals output onset mask.
- [ ] Input continuation mask equals output continuation mask.
- [ ] Identical initial state produces identical melodic intent.
- [ ] Auto selection reaches more than one legal contour/operation over the
      deterministic test matrix.
- [ ] Degree and maximum-leap bounds are never violated.
- [ ] No heap allocation or global RNG is introduced.
- [ ] No Scene, Phrase, 15A-history, bass, or voice-allocation ownership appears.
- [ ] Production reachability remains explicitly unclaimed until Stage 14 wiring
      is stabilized and separately reviewed.
- [ ] Hardware musical verdict remains **PENDING**.
