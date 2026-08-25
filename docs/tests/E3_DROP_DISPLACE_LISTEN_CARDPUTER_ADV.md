# 0.9.9-E3L — DROP / DISPLACE musical listening on Cardputer ADV

## Purpose

`E3 LISTEN` is disposable REVIEW / HARDWARE firmware for listening to the
frozen 0.9.9-E3R-B cap=1 corpus as:

```text
1 = C  canonical
2 = V  before the current operation
3 = W  candidate after the current operation
```

This branch does **not** enable production DROP or DISPLACE, does not add a
selector/lifecycle, does not change ReferenceVocabulary policy, and does not
introduce a new audio renderer.

Exact frozen source base:

```text
fd39cc410075268b66fae9d87c1eb8ffb41026bc
```

Review branch:

```text
agent/20260825-03-0.9.9-e3-musical-listen-surface
```

For a physical listening report, also record the exact flashed review HEAD:

```bash
git rev-parse HEAD
```

## Hardware

- M5Stack Cardputer ADV (ESP32-S3)
- USB-C data cable
- built-in speaker for the primary acceptance
- optional SEQTRAK through GroovePuter's existing MIDI route

## Wiring

No special wiring is required.

The E3 listener does not use PORT.A or any new I2C/SPI peripheral. Built-in
Cardputer audio is the default. If SEQTRAK is used, use the already-established
GroovePuter MIDI connection; E3L adds no MIDI protocol.

## Frozen 32-case provenance

The build reproduces the existing E3R-B cap=1 report and refuses to build unless
all four frozen artifact SHA-256 values match:

```text
summary CSV
9c9d3983f456e8ef3ffe19422c08cba0dbaafcb9470ab4f1d33d9b6482b98ed1

summary JSON
bbad8865638cc3dc620680b806cd4a6c00da13fb1f335383b6be0d569356abe5

review corpus CSV
6216accb1d399dfe8d909646980b0cfee04fcb63d2dd4a88535cc52a6217fd7d

review corpus MD
edf2b8c0bf2bec8944648870be156fa237243a24fe0f8fa7fb6d6dc985f23ecb
```

The generated fixture must contain exactly:

```text
DROP       12
DISPLACE   12
COMBINED    8
TOTAL      32

BassRhythm mutated role
15 / 32
```

No new random corpus is selected. C, V and W are embedded as complete frozen
role masks (`structural`, `secondary`, `ghost`, gate masks and accents). E3L
does not execute DROP or DISPLACE to construct W.

The generator also fails closed if any frozen C/V/W case changes
`ChordRhythm` or `MelodicRhythm`, because this review branch has no approved
arbitrary `RhythmPhrasePlan -> physical Synth B` boundary.

## Audibility classification

Every screen/Serial case is intentionally labelled:

```text
PRODUCTION_CONTEXT_AUDITION
```

Do not describe E3L as byte-exact full-mix E3R-B playback.

Why: current production has no authoritative generic physical conversion from
all `RhythmPhrasePlan` synth-role fields to the later role-specific tonal
owners.

For drum-mutated cases the screen also reports:

```text
role: EXACT
```

That narrower statement is valid: the mutated physical drum role comes from the
exact frozen C/V/W masks and is sent through the existing production
`materializeRhythmPattern()` + `standardDrumPatternBinding()` + Feel path.

For BassRhythm cases the screen reports:

```text
role: CONTEXT
```

Bass timing is audible through a review-only production-context seam described
below.

## Drum materialization

Physical drum roles use the normal production owner:

```text
frozen C/V/W RhythmPhrasePlan
        |
        v
materializeRhythmPattern(...)
        |
standardDrumPatternBinding(...)
        |
existing Feel application
        |
normal GroovePuter drum playback
```

The standard binding remains:

```text
Kick        -> KICK
Backbeat    -> SNARE
ClosedHat   -> CLOSED_HAT
OpenHat     -> OPEN_HAT
Percussion  -> RIM
```

No E3 drum renderer exists.

C/V/W share the same review BPM, Feel policy, downstream context and mute state
unless isolation is explicitly enabled.

## BassRhythm materialization boundary

Production bass ownership is:

```text
realizeBassRhythm()
    ->
realizeBassPitchBehavior()
    ->
tonal materialization
    ->
adaptTonalPlanToSynthPattern()
    ->
Feel
    ->
Synth A playback
```

The disposable staged build inserts the E3L seam at the narrowest boundary:

```text
realizeBassRhythm()
    ->
[E3L: replace only BassRhythmPlan onset timing from selected frozen C/V/W]
    ->
realizeBassPitchBehavior()
    ->
existing tonal / register / scale / Synth A / Feel owners
```

The tracked repository `src/` file is not changed. The seam exists only in the
temporary staged sketch created by `scripts/build_e3_listen.sh`.

Important limitation: `RhythmPhrasePlan::BassRhythm` carries importance and
gate masks, while `BassRhythmPlan` has onset/continuation topology and no frozen
contract mapping those gate masks to continuations. E3L therefore does **not**
guess such a mapping. It injects the frozen onset union and sets review
continuations to zero. That is why Bass playback is
`PRODUCTION_CONTEXT_AUDITION`, not exact-plan playback.

E3L never injects fixed MIDI notes, SynthStep notes, a click substitute or a
second tonal materializer.

## Deterministic review context

C, V and W use the same review context:

```text
BPM                  124
Feel                 Straight / amount 0
generation attempt   0
pattern address       7
root pitch class      C / 0
tonal materialization enabled
downstream profile    fixed review context
```

The frozen E3R-B archetype family is pinned only in the staged review build.
Existing production role owners remain responsible for downstream pitch,
progression, register, velocity, timbre and Feel behavior.

## Review sandbox

E3L uses reserved, destructive review storage:

```text
current physical pattern page
Bank B
pattern 1
Song B
rows 1..4
```

The same selected C/V/W physical material is written to all four Song rows.

Do not keep important material in this review location while running E3L.

On entry the page captures:

- transport play/stop state
- BPM
- Song mode
- loop enable/range
- active and playback Song slots
- Song position
- drum bank/pattern
- Synth A bank/pattern
- Synth B bank/pattern
- all ten track mute states

On exit these runtime selections are restored. The contents overwritten in the
declared Bank B/Song B sandbox are intentionally destructive, matching the F08
review precedent.

The E3 LISTEN page id is never persisted into the normal UI session.

## Build / Flash

Install dependencies if needed:

```bash
bash scripts/install_arduino_deps.sh
```

Validate the host/review contract:

```bash
bash tests/run_0_9_9_e3_listen_tests.sh
```

Build the disposable firmware:

```bash
bash scripts/build_e3_listen.sh --warnings all
```

Expected binary:

```text
build/cardputer-adv-e3-listen/GroovePuter.ino.bin
```

Flash:

```bash
BUILD_PATH="$PWD/build/cardputer-adv-e3-listen" \
  bash scripts/upload.sh /dev/ttyACM0
```

The ordinary repository build remains separate:

```bash
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh \
  build/cardputer-adv-current/GroovePuter.ino.elf
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

Review-only fixture bytes are not a production DRAM-budget regression.

## Controls

```text
Ctrl+V        enter / exit E3 LISTEN

Left / Right  previous / next case
Up / Down     previous / next DROP / DISPLACE / COMBINED group

1             play C — CANONICAL
2             play V — BEFORE
3             play W — AFTER

G             replay selected side from bar 0
Space         play / stop

I             isolate mutated physical role
I again       restore FULL MIX
```

`FULL MIX` is the default and the primary acceptance mode.

Every C/V/W selection performs the same restart contract:

```text
stop
-> install selected side
-> Song row/bar 0
-> four identical review bars
-> loop rows 0..3
-> start
```

A side is never switched halfway through a bar.

## Screen

Typical DROP case:

```text
E3 LISTEN
TEST ONLY                         04/32
DROP  HalfTime  P3
DROP Kick  14 -> OFF
src ADDED/SECONDARY  density 12>11
CTX AUDITION  role:EXACT
[1]C [2]V [3]W   PLAY:W >
FULL MIX  bar0 x4  I:isolate
```

Typical BassRhythm DISPLACE:

```text
DISPLACE  Rolling  P3
DISPLACE Bass  12 -> 10  d2
...
CTX AUDITION  role:CONTEXT
```

Serial emits one diagnostic line per applied side. Serial is not required to
operate the test.

## Full mix / isolation

Primary judgement is always `FULL MIX`.

Use `I` only to diagnose a subtle change:

```text
BassRhythm  -> Synth A only
Kick        -> KICK only
Backbeat    -> SNARE only
ClosedHat   -> CLOSED_HAT only
OpenHat     -> OPEN_HAT only
Percussion  -> RIM only
```

Return to `FULL MIX` before assigning the final grade.

## Listening procedure

For each case:

1. Listen to `C` for at least two loops.
2. Press `2` and listen to `V`.
3. Press `3` and listen to `W`.
4. Repeat `1 -> 2 -> 3` if the effect is unclear.
5. Use `I` briefly if the mutation is masked by the mix.
6. Return to `FULL MIX`.
7. Record `GOOD`, `BORDERLINE` or `BAD`, plus a short reason.
8. Move to the next case.

Mandatory questions:

```text
DROP
- does it create space or only a hole?
- is added-material DROP natural?
- is canonical structural DROP destructive?

DISPLACE
- does distance 2 sound like groove motion or a timing error?
- is structural displacement acceptable?
- is secondary displacement acceptable?
- does BassRhythm retain phrase identity?

COMBINED
- evolution or random walk?
- does the pattern remain recognizably related?
```

Special evidence that must be heard in the frozen set:

```text
P2 canonical DROP
P3 added/noncanonical DROP
DISPLACE distance 2
structural DISPLACE
secondary DISPLACE
BassRhythm DISPLACE
Rolling/P3 combined case
```

## Listening matrix

Fill one row per frozen case from the on-device case id:

| # | Case ID | C/V/W grade | FULL MIX reason | Isolation note |
|---:|---|---|---|---|
| 01 | | GOOD / BORDERLINE / BAD | | |
| 02 | | GOOD / BORDERLINE / BAD | | |
| 03 | | GOOD / BORDERLINE / BAD | | |
| 04 | | GOOD / BORDERLINE / BAD | | |
| 05 | | GOOD / BORDERLINE / BAD | | |
| 06 | | GOOD / BORDERLINE / BAD | | |
| 07 | | GOOD / BORDERLINE / BAD | | |
| 08 | | GOOD / BORDERLINE / BAD | | |
| 09 | | GOOD / BORDERLINE / BAD | | |
| 10 | | GOOD / BORDERLINE / BAD | | |
| 11 | | GOOD / BORDERLINE / BAD | | |
| 12 | | GOOD / BORDERLINE / BAD | | |
| 13 | | GOOD / BORDERLINE / BAD | | |
| 14 | | GOOD / BORDERLINE / BAD | | |
| 15 | | GOOD / BORDERLINE / BAD | | |
| 16 | | GOOD / BORDERLINE / BAD | | |
| 17 | | GOOD / BORDERLINE / BAD | | |
| 18 | | GOOD / BORDERLINE / BAD | | |
| 19 | | GOOD / BORDERLINE / BAD | | |
| 20 | | GOOD / BORDERLINE / BAD | | |
| 21 | | GOOD / BORDERLINE / BAD | | |
| 22 | | GOOD / BORDERLINE / BAD | | |
| 23 | | GOOD / BORDERLINE / BAD | | |
| 24 | | GOOD / BORDERLINE / BAD | | |
| 25 | | GOOD / BORDERLINE / BAD | | |
| 26 | | GOOD / BORDERLINE / BAD | | |
| 27 | | GOOD / BORDERLINE / BAD | | |
| 28 | | GOOD / BORDERLINE / BAD | | |
| 29 | | GOOD / BORDERLINE / BAD | | |
| 30 | | GOOD / BORDERLINE / BAD | | |
| 31 | | GOOD / BORDERLINE / BAD | | |
| 32 | | GOOD / BORDERLINE / BAD | | |

Useful final summary format:

```text
P2 CANONICAL DROP       FAIL
P3 ADDED DROP           PASS
DISPLACE STRUCTURAL D2  PASS / RESTRICT
DISPLACE SECONDARY D2   PASS
BASS DISPLACE D2        PASS / RESTRICT
DROP+DISPLACE           PASS / TOO ACTIVE
```

## Expected behavior

After `Ctrl+V`:

- case `01/32` starts on `C`;
- sound starts from Song row/bar 0;
- the same material repeats for four bars;
- `1`, `2`, `3` each stop and restart deterministically;
- `Left/Right` changes case and resets to `C`;
- `Up/Down` jumps between review categories;
- `I` changes only the listening mix, not the generated fixture;
- exiting restores the captured runtime selections and mute state.

Normal Genre generation outside E3 LISTEN remains the frozen E3R-B/E3a behavior.
Production proposal counts remain unchanged: DROP and DISPLACE are not enabled
by this review surface.

## Troubleshooting

### Build fails on an E3R-B SHA mismatch

Expected. E3L is fail-closed. Do not update the digest casually. Re-run/review
E3R-B as a separate research decision if the frozen authority intentionally
changes.

### Build says ChordRhythm or MelodicRhythm has no approved physical boundary

Expected fail-closed behavior. The selected frozen corpus now requires a
physical synth-role interpretation that E3L is not allowed to invent. Do not add
fixed notes or a second renderer to bypass it.

### `Ctrl+V` does not open E3 LISTEN

Confirm the flashed binary came from:

```text
build/cardputer-adv-e3-listen
```

not the normal build. The Cardputer firmware has the established Ctrl+letter
input path; E3L uses that path rather than an unproven triple-modifier chord.

### `E3 FIXTURE FAILED`

The production-context migration could not materialize the selected frozen case.
Do not reinterpret the case. Rebuild and inspect the `[E3-LISTEN]` Serial line
and build-time frozen-corpus checks.

### I hear silence

Exit E3 LISTEN and verify ordinary GroovePuter audio first. Re-enter the page.
E3L starts in FULL MIX and explicitly unmutes review tracks while active, then
restores the prior mute state on exit.

### My Bank B / Song B material changed

Expected. That is the declared destructive review sandbox.

## Acceptance checklist

- [ ] Flashed source ancestry starts from `fd39cc410075268b66fae9d87c1eb8ffb41026bc`.
- [ ] Exact flashed review HEAD SHA is recorded.
- [ ] All four frozen E3R-B artifact SHA-256 values verify.
- [ ] Generated review corpus is exactly 32 cases: 12 DROP, 12 DISPLACE, 8 COMBINED.
- [ ] Generated corpus contains exactly 15 mutated `BassRhythm` cases.
- [ ] Build fails rather than inventing ChordRhythm/MelodicRhythm physical semantics.
- [ ] No tracked production `src/` file contains the E3L review hook.
- [ ] E3L contains no second DROP/DISPLACE executor.
- [ ] `Ctrl+V` enters and exits E3 LISTEN.
- [ ] `Left/Right` walks all 32 cases and resets each new case to C.
- [ ] `1` plays C, `2` plays V, `3` plays W.
- [ ] Every C/V/W change restarts from bar 0 and repeats four bars.
- [ ] `G` deterministically replays the selected side from bar 0.
- [ ] `Space` stops/starts review playback.
- [ ] FULL MIX is the default.
- [ ] `I` isolates the mutated physical role and toggles back to FULL MIX.
- [ ] Drum mutations use existing production `materializeRhythmPattern()` / standard binding / Feel.
- [ ] Bass timing enters after `realizeBassRhythm()` and before `realizeBassPitchBehavior()`.
- [ ] Bass downstream pitch/tonal/Synth A/Feel owners remain production owners.
- [ ] Screen/Serial says `CTX AUDITION`; no case is falsely called byte-exact full-mix playback.
- [ ] Previous BPM, transport, Song/pattern selections, loop state and mutes return after exit.
- [ ] Normal Cardputer ADV build remains green.
- [ ] Normal fixed DRAM check remains green.
- [ ] Normal SEQTRAK MIDI-only build remains green.
- [ ] Physical grades are recorded for all 32 cases.
- [ ] E3R-B remains frozen.
- [ ] Production DROP remains disabled.
- [ ] Production DISPLACE remains disabled.
- [ ] E3b has not started.
