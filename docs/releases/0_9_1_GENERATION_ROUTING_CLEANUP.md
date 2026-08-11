# 0.9.1 Generation Routing Cleanup

## Purpose

Make the Stage 15 hardware audition unambiguous before release acceptance.

This checkpoint fixes command ownership only. It does not redesign P1/P2/P3,
add musical vocabulary, or add a new single-synth Stage 15 generator.

Release-facing routing:

```text
GENRE G          full Stage 15 materialization
DRUMS G          drums-only strong generation
DRUMS Ctrl+G     selected drum voice randomize
DRUMS Alt+G      deliberate CHAOS randomize
DRUMS Ctrl+Alt+G Stage 12 1/2/4/8-bar audition/probe
```

The old sketch-level I/O/P generation fallthrough is blocked on the GENERATE
workflow. On the DRUMS main grid, O/P are also blocked; I remains a valid Q-I
pattern-slot key.

## Hardware list

- M5Stack Cardputer ADV / ESP32-S3.
- USB-C data cable.
- Built-in speaker or the normal GroovePuter audio output.
- Optional Yamaha SEQTRAK for MIDI smoke testing after the local audition.

No external I2C/SPI accessory is required.

## Wiring

No external wiring is required.

- Power/program the Cardputer ADV over USB-C.
- PORT.A is untouched by this change.
- If PORT.A I2C hardware is already connected, keep the project invariant:
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

Then require the normal release build matrix on the same commit:

```text
SDL
Cardputer ADV normal
Cardputer ADV fixed DRAM
Cardputer ADV SEQTRAK MIDI-only
Stage 15 tonal acceptance workflows
```

Flash the exact unchanged PR head used by those checks. Do not audition a newer
local commit while quoting CI from an older SHA.

## Expected behavior

### GENRE

Select GENRE / VARIANT / RHYTHM, then press plain `G`.

Expected:

```text
G -> regenerateWithStrongRhythmMigration()
```

The pending GENRE / VARIANT / RHYTHM selection is committed before generation.
`G` always materializes, even when the ENTER APPLY selector is `PROFILE ONLY`.
ENTER itself keeps its existing APPLY-mode behavior.

Plain `I`, `O`, and `P` must not reach the legacy single-synth/drum generators on
this page.

### FEEL

Plain `I`, `O`, and `P` must not reach the old sketch-level generation fallback.

`P` shows:

```text
CONTINUE: Ctrl+Alt+G
```

I/O report that legacy synth generation is disabled for this release surface.

### DRUMS main grid

Plain `G` regenerates only drums through the selected strong rhythm + FEEL path.
Synth A/B must remain unchanged.

`Ctrl+G` keeps the selected-drum-voice randomizer.

`Alt+G` keeps CHAOS. CHAOS is intentionally outside P1/P2/P3 and is not expected
to reproduce the selected RHYTHM topology.

`Ctrl+Alt+G` starts the explicit Stage 12 audition/probe. FEEL `REPEATS` selects
1/2/4/8 bars. The command reserves current-page Bank B and Song B for audition,
then uses the existing Song transport.

Expected toast examples:

```text
AUD 4B EVOLVED #<id>
AUD 4B VARIATION #<id>
```

If Song mode is already active:

```text
AUD: EXIT SONG
```

Plain `P` must not mutate the drum pattern. It points to `Ctrl+Alt+G` instead.
Plain `O` must not invoke legacy Synth B generation on this page.

`I` remains a normal Q-I pattern-slot key on the DRUMS grid.

### Comparing GENRE G and DRUMS G

Do not compare the complete audible output for equality:

```text
GENRE G -> drums + Synth A + Synth B
DRUMS G -> drums only
```

For the same active GENRE / VARIANT / RHYTHM / FEEL and pattern address, verify
that the generated drum result belongs to the same selected rhythm identity and
genre corridor. Synth differences are expected because DRUMS G intentionally
leaves both synth patterns untouched.

### Serial

`Ctrl+Alt+G` on Cardputer ADV prints a line beginning with:

```text
[PHRASE-PROBE]
```

Record the complete line for at least one 4-bar and one 8-bar run. It includes
command duration, worst Reduction/Break timing, task stack high-water and
internal heap/largest-block values.

## Troubleshooting

### P still changes drums

The tested firmware is not this routing candidate, or the event reached a stale
build. Confirm the exact flashed SHA. On this checkpoint, DRUMS `P` is consumed
before the sketch-level `randomizeDrumPattern()` fallback.

### O still generates Synth B on GENRE/FEEL/DRUMS

Confirm the exact flashed SHA. The release-facing pages consume the stale O
shortcut before it can reach `randomize303Pattern(1)`.

### Ctrl+Alt+G behaves like Ctrl+G

The Stage 12 audition route is missing from the build or the modifier state is
not reaching the DRUMS main-page wrapper. Capture the `[KEY]` Serial line and
verify both `ctrl=1` and `alt=1` for G.

### GENRE G and DRUMS G sound different

That is expected at the full-mix level. GENRE G regenerates all three material
lanes; DRUMS G changes only drums. Compare the drum identity and selected
GENRE/RHYTHM relationship, not the full mix.

### No PHRASE-PROBE line

Use `Ctrl+Alt+G` from the DRUMS main grid while not already in Song mode. Plain G
is intentionally still the one-bar production route and does not run the probe.

## Acceptance checklist

- [ ] Focused Stage 12 tests pass on the exact head.
- [ ] Stage 13/14/15 generation matrix passes on the exact head.
- [ ] Full host suite passes on the exact head.
- [ ] SDL build passes.
- [ ] Cardputer ADV normal build passes.
- [ ] Cardputer ADV fixed-DRAM gate passes.
- [ ] Cardputer ADV SEQTRAK MIDI-only build passes.
- [ ] Stage 15 tonal workflows pass on the same SHA.
- [ ] GENRE G regenerates the complete current Stage 15 material.
- [ ] GENRE I/O/P do not invoke legacy generation.
- [ ] FEEL I/O/P do not invoke legacy generation.
- [ ] DRUMS G changes drums and leaves Synth A/B unchanged.
- [ ] DRUMS Ctrl+G changes only the selected drum voice.
- [ ] DRUMS Alt+G still produces intentional CHAOS behavior.
- [ ] DRUMS P does not modify the pattern and points to Ctrl+Alt+G.
- [ ] DRUMS O does not invoke legacy Synth B generation.
- [ ] DRUMS I still selects its Q-I pattern slot.
- [ ] Ctrl+Alt+G is recognized on physical Cardputer ADV.
- [ ] FEEL REPEATS 1/2/4/8 maps to audition length 1/2/4/8.
- [ ] Bank A and Song A remain untouched by audition.
- [ ] Leaving audition restores the pre-audition Drum/Synth bank+pattern selection.
- [ ] A complete 4-bar `[PHRASE-PROBE]` line is captured.
- [ ] A complete 8-bar `[PHRASE-PROBE]` line is captured.
- [ ] No reset, watchdog, new audio underrun, or unacceptable UI/audio stall occurs.
- [ ] P1/P2/P3 redesign remains deferred to a separate post-routing change.
- [ ] Three consecutive clean reviews are completed on one unchanged final SHA.
