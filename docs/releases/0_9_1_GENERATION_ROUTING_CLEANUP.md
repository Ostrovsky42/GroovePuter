# 0.9.1 Generation Routing Cleanup

## Purpose

Freeze one unambiguous release-facing generation command map for the Stage 15
0.9.1 candidate.

This checkpoint began as routing-only work in #226. The stacked #227 P-level
selector is now part of the same branch, so `P` has a production owner and is no
longer a continuation hint.

Current routing:

```text
GENRE P          cycle P1 -> P2 -> P3
GENRE G          full Stage 15 materialization at current P-level
FEEL P           cycle the same shared P-level
DRUMS P          cycle the same shared P-level
DRUMS G          drums-only strong generation at current P-level
DRUMS Ctrl+G     selected drum voice randomize
DRUMS Alt+G      deliberate CHAOS randomize; outside P1/P2/P3
DRUMS Ctrl+Alt+G Stage 12 1/2/4/8-bar audition/probe at current P-level
```

The retained sketch-level legacy `I/O/P` generator fallback is not deleted in
this checkpoint. Release-facing pages take first ownership instead:

- GENRE/FEEL consume legacy `I/O` before the sketch fallback;
- GENRE/FEEL/DRUMS own `P` through the shared P-level request selector;
- DRUMS consumes legacy `O`;
- DRUMS `I` remains the normal Q-I pattern-slot key.

P-level semantics and listening details are documented in
`docs/releases/0_9_1_P_LEVEL_PRODUCTION_SELECTOR.md`.

## Hardware list

- M5Stack Cardputer ADV / ESP32-S3.
- USB-C data cable.
- Built-in speaker or the normal GroovePuter audio output.
- Optional Yamaha SEQTRAK for the final MIDI smoke test.

No external I2C/SPI accessory is required.

## Wiring

No external wiring is required.

- Power/program the Cardputer ADV over USB-C.
- PORT.A is untouched.
- If PORT.A I2C hardware is already connected, preserve:
  - SDA GPIO2
  - SCL GPIO1

## Build / Flash steps

Run the focused generation gates:

```bash
bash tests/run_phrase_stage12_tests.sh
bash tests/run_generation_stage13_tests.sh
```

Run the repository aggregate:

```bash
bash tests/run_host_tests.sh
```

Then require the normal release matrix on the same final commit:

```text
Phrase Core
Synth persistence
Tonal Projector
Stage 15B
Stage 15C
Scale quantization correctness
Stage 15 Tonal Materializer
Stage 15 tonal integration
Stage 15 tonal register sweep
Stage 15 tonal global scale
Stage 15 final tonal acceptance
SDL
Cardputer ADV normal + fixed DRAM
Cardputer ADV SEQTRAK MIDI-only
```

The former Stage 12 source assertion that categorically forbade
`phrase_evolution` from the live bridge is obsolete once #226 intentionally
restores the explicit `Ctrl+Alt+G` audition owner. The current source gate must
instead prove both facts:

1. `regeneratePhraseAuditionWithProbe()` may call multi-bar Stage 12 evolution;
2. normal GENRE `G`, DRUMS `G`, and `strong_rhythm_migration.cpp` remain one-bar
   and do not call `evolveMultiBarPhrase()` directly.

Do not accept a red Stage 12 gate as a release exception. The final combined
head must use the updated contract and pass the aggregate host suite.

Flash the exact final release candidate selected after those checks.

## Expected behavior

### GENRE

Select GENRE / VARIANT / RHYTHM.

`P` cycles:

```text
P1 CANON -> P2 VAR -> P3 TRANS -> P1 CANON
```

Changing P-level alone must not mutate the current pattern.

Plain `G` commits the pending GENRE / VARIANT / RHYTHM selection and calls:

```text
regenerateWithStrongRhythmMigration()
```

It materializes the complete current Stage 15 groove at the selected P-level,
even when ENTER's APPLY selector is `PROFILE ONLY`.

Plain `I/O` must not reach the legacy single-synth generators on GENRE.

### FEEL

`P` cycles the same shared P-level request state.

Plain `I/O` must not reach the old sketch-level generation fallback.

Changing FEEL or P-level alone does not materialize a new groove; generation
still requires `G` or the explicit phrase audition command.

### DRUMS main grid

Plain `G` regenerates only drums through the selected strong rhythm + FEEL path
at the current P-level. Synth A/B must remain unchanged.

`Ctrl+G` keeps selected-drum-voice randomize.

`Alt+G` keeps deliberate CHAOS and remains outside P1/P2/P3.

`P` cycles the same shared P-level and must not mutate the current drum pattern
by itself.

`Ctrl+Alt+G` starts the explicit Stage 12 audition/probe. FEEL `REPEATS` selects
1/2/4/8 bars. The command reserves current-page Bank B and Song B for audition,
then uses the existing Song transport.

`O` must not invoke legacy Synth B generation.

`I` remains a normal Q-I pattern-slot key.

### Comparing GENRE G and DRUMS G

Do not compare the complete audible output for equality:

```text
GENRE G -> drums + Synth A + Synth B
DRUMS G -> drums only
```

For the same active GENRE / VARIANT / RHYTHM / FEEL / P-level and pattern
address, compare the drum identity/corridor. Synth differences are expected
because DRUMS G intentionally leaves both synth patterns untouched.

### Stage 12 audition

Expected toast examples:

```text
AUD 4B P1 CANON EVOLVED #<id>
AUD 4B P2 VAR EVOLVED #<id>
AUD 4B P3 TRANS EVOLVED #<id>
AUD 4B P2 VAR VARIATION #<id>
```

A one-bar-only identity may legally use the deterministic variation fallback.

If Song mode is already active:

```text
AUD: EXIT SONG
```

### Serial

`Ctrl+Alt+G` prints a line beginning with:

```text
[PHRASE-PROBE]
```

The line includes `level=P1|P2|P3`, requested bars, selected archetype,
trajectories, command duration, worst Reduction/Break timing, task stack
high-water, internal free heap, and largest internal block.

## Troubleshooting

### P changes the pattern immediately

Fail. `P` is selector-only. Pattern materialization still requires `G` or
`Ctrl+Alt+G`.

### P still shows CONTINUE

The firmware predates the stacked P-level selector. Confirm the flashed branch
and SHA. The final #226 branch owns P as `P1/P2/P3` selection.

### O still generates Synth B on GENRE/FEEL/DRUMS

Confirm the exact flashed candidate. Release-facing pages must consume the old O
shortcut before it reaches `randomize303Pattern(1)`.

### Ctrl+Alt+G behaves like Ctrl+G

Capture the `[KEY]` Serial line and verify both `ctrl=1` and `alt=1` for G. The
explicit audition owner must run before selected-voice randomize.

### Stage 12 host test rejects phrase_evolution in the bridge

The checkout still contains the pre-#226 source contract. The current contract
allows Stage 12 only through `regeneratePhraseAuditionWithProbe()` and continues
to forbid multi-bar evolution in the normal one-bar production migration path.

### GENRE G and DRUMS G sound different

Expected at full-mix level. GENRE G regenerates all three material lanes; DRUMS
G changes only drums.

### No PHRASE-PROBE line

Use `Ctrl+Alt+G` from the DRUMS main grid while not already in Song mode. Plain
G intentionally remains the normal one-bar production route.

## Acceptance checklist

- [ ] Focused Stage 12 tests pass on the final combined head.
- [ ] Stage 13/14/15 generation matrix passes.
- [ ] Full `tests/run_host_tests.sh` passes; no inherited Stage 12 exception is carried forward.
- [ ] Phrase Core passes.
- [ ] Synth persistence passes.
- [ ] Tonal Projector and all Stage 15 tonal gates pass.
- [ ] SDL build passes.
- [ ] Cardputer ADV normal build passes.
- [ ] Cardputer ADV fixed-DRAM gate passes.
- [ ] Cardputer ADV SEQTRAK MIDI-only build passes.
- [ ] GENRE P / FEEL P / DRUMS P operate one shared P-level selector.
- [ ] Pressing P alone never mutates current material.
- [ ] GENRE G regenerates complete Stage 15 material at the selected P-level.
- [ ] GENRE/FEEL I/O do not invoke legacy synth generation.
- [ ] DRUMS G changes drums and leaves Synth A/B unchanged.
- [ ] DRUMS Ctrl+G changes only the selected drum voice.
- [ ] DRUMS Alt+G remains deliberate CHAOS outside P1/P2/P3.
- [ ] DRUMS O does not invoke legacy Synth B generation.
- [ ] DRUMS I still selects its Q-I pattern slot.
- [ ] Ctrl+Alt+G is recognized on physical Cardputer ADV.
- [ ] Ctrl+Alt+G consumes the selected P-level.
- [ ] FEEL REPEATS 1/2/4/8 maps to audition length 1/2/4/8.
- [ ] Bank A and Song A remain untouched by audition.
- [ ] Leaving audition restores pre-audition Drum/Synth bank+pattern selection.
- [ ] `[PHRASE-PROBE]` reports `level=P1|P2|P3` and runtime metrics.
- [ ] No reset, watchdog, new audio underrun, or unacceptable UI/audio stall occurs.
- [ ] Three consecutive clean reviews are completed on one unchanged final SHA after the last finding/fix.
