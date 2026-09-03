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

## Characterized contracts

### GRID

`FeelSettings.gridSteps` remains persisted as 8/16/32, but `MiniAcid::processSequencerEvents()` does not consume it. Current synth playback remains the fixed 16-step scheduler:

```text
384 ticks / bar
24 ticks / physical step
16 physical steps / Pattern
```

The runtime test executes the same deterministic Synth A and Synth B Pattern under GRID 8, 16 and 32 and requires identical USB/DIN event traces.

### Exact bar boundary

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

### Song physical-pattern transition

Current behavior is asymmetric and this is intentional characterization, not desired PHRASE semantics.

At an exact 384-tick Song row transition with `patternBars=1`:

```text
PatternPlayer MIDI ownership -> released by publishPatternAllNotesOff_()
direct internal synth voice  -> not synchronously released by applySongPositionSelection()
```

The test holds an internal gate artificially active through the transition and proves this divergence on both Synth A and Synth B. Future Phrase lifetime work must replace this with one backend-independent logical lifetime decision; P0 does not fix it.

### Legacy TIE

`SynthStep::note == -2` remains a compatibility sentinel. If an active gate reaches a TIE tick, `triggerSynthStep_()` extends the existing gate countdown. P0 demonstrates the reachable cross-boundary symptom:

```text
step15 onset timing +23 -> barTick 383
physical boundary       -> 384
step0 TIE timing +1     -> barTick 1 / absolute tick 385
```

The gate is extended, while no new MIDI NoteOn is emitted.

This is **not** authoritative Phrase lifetime representation.

### Swing / microtiming modulo wrap

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

These are scheduler-position wraps. They do not establish phrase-wide note ownership.

### Synth A / Synth B and USB / DIN

Every lifetime/timing case is executed for both synth tracks. MIDI acceptance uses one real `UsbMidiOutput` over the real `TeeMidiTransport` with accepting fake USB and DIN transports. The two endpoint traces must be byte-identical for every characterized case.

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

Stop. That means current post-#419 behavior differs from the historical M2 characterization. Record the new exact behavior instead of forcing the old expectation.

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
- [ ] legacy TIE crossing symptom frozen without promoting `-2` to Phrase semantics
- [ ] swing-only 372 coordinate frozen
- [ ] swing + microtiming modulo wrap to tick 11 frozen
- [ ] negative step0 microtiming wrap to tick 361 frozen
- [ ] USB/DIN accepting-endpoint traces identical
- [ ] deterministic GCC repeat PASS
- [ ] Clang parity PASS when available
- [ ] ASan PASS
- [ ] UBSan PASS
- [ ] no production implementation in this PR

## Decision after GREEN

```text
CURRENT PATTERN BEHAVIOR CHARACTERIZED  YES
PRODUCTION SEMANTIC DELTA              NONE
PHRASE LIFETIME IMPLEMENTATION         NOT STARTED
NEXT                                   IMMUTABLE EVENT PROJECTION CONTRACT
```
