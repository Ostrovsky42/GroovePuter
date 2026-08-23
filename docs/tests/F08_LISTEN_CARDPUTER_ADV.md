# F08 LISTEN — Cardputer ADV hardware acceptance

## Purpose

`F08 LISTEN` is a **test-only** A/B listening surface for the 11 frozen
0.9.9-F08 harmonic-rhythm review cases.

It does not add a production harmonic-rhythm mode and does not restore the old
`ChordRhythm -> HarmonicRhythm` ownership path. The review build generates OLD
fixtures on the host from an exact temporary reverse of the F08 patch, generates
NEW fixtures from the current F08 source, validates the pair, then embeds only
the frozen physical pattern bytes in the review firmware.

Controls:

```text
Left / Right   previous / next case
A              play OLD
B              play NEW
G              replay current side
Ctrl+F         enter / exit F08 LISTEN
```

Changing case always starts that case on `OLD`.

`Ctrl+F` is intentional: the Cardputer input layer has an explicit tested
Ctrl+letter HID path. The earlier `Ctrl+Alt+F` review chord was removed after it
proved unreliable on the physical keyboard.

## Hardware list

- M5Stack Cardputer ADV (ESP32-S3, no PSRAM configuration used by GroovePuter)
- USB-C data cable
- Built-in speaker, or the normal GroovePuter audio path you already use

## Wiring

No external wiring is required.

`F08 LISTEN` does not use PORT.A, I2C, or external displays.

## Build / Flash

Checkout the review branch, then:

```bash
bash scripts/install_arduino_deps.sh
bash scripts/build_f08_listen.sh --warnings all
```

The dedicated binary is written to:

```text
build/cardputer-adv-f08-listen/GroovePuter.ino.bin
```

Flash it:

```bash
BUILD_PATH="$PWD/build/cardputer-adv-f08-listen" \
  bash scripts/upload.sh /dev/ttyACM0
```

If your Cardputer appears on another serial device, replace `/dev/ttyACM0`.

## Review sandbox

This firmware intentionally overwrites only the review sandbox while the page is
used:

```text
current pattern page
Bank B
pattern 1
Song B
row 1
```

Do not keep important material in that location while running the test.

On exit, the page restores the previous BPM, transport state, Song mode/slot,
loop range, pattern bank/selection, and all track mute states. The overwritten
Bank B pattern 1 / Song B row 1 content itself is deliberately not restored.

## Cases

| # | Case | OLD -> NEW | Focus |
|---:|---|---|---|
| 1 | DnB 5 B / MINOR FALL | `0000 -> 8080` | added movement |
| 2 | TripHop 4 A / II-V-I | `0000 -> 8080` | added movement |
| 3 | House 4 A / POP CYCLE | `0000 -> 8080` | added movement |
| 4 | House 5 B / POP CYCLE | `4904 -> 8080` | reduced activity |
| 5 | Outrun 0 B / POP CYCLE | `2448 -> 8080` | reduced activity |
| 6 | UKG 1 B / BORROWED LIFT | `0101 -> 8080` | relocated timing |
| 7 | FunkSoul 6 B / BORROWED LIFT | `0802 -> 8080` | relocated timing |
| 8 | TripHop 2 B / PARALLEL SHIFT | `0902 -> 8080` | chord phrasing |
| 9 | Acid 2 B / STATIC MODAL | unchanged | static negative control |
| 10 | Techno 4 B / PEDAL DRONE | unchanged | static negative control |
| 11 | Reggae 4 B / BORROWED LIFT | `0202 -> 8080`, output same | sensitivity control |

The screen shows `CHANGED` or `SAME` for the selected frozen corpus row. For
controls 9, 10 and 11, `SAME` is intentional.

## Listening procedure

1. Boot the review firmware.
2. Press `Ctrl+F`.
3. Case `01/11` starts automatically on `OLD`.
4. Listen for at least two loops.
5. Press `B`; listen to `NEW`.
6. Press `A`, then `B` again if the difference is unclear.
7. Judge only by ear; do not treat the hexadecimal clock as the expected answer.
8. Press `Right` for the next case. It starts on `OLD`.
9. Use `G` whenever you want the same side restarted from step 1.
10. Press `Ctrl+F` to leave the review surface.

For each case report:

```text
DnB 5 B / MINOR FALL
A progression: PASS
B movement natural: BORDERLINE
C no step-8 artifact: FAIL
D role coherence: PASS
note: change sounds too predictable
```

Interpretation:

```text
A — progression recognisable
B — harmonic movement natural
C — no artificial mandatory step-8 turn
D — bass/chords/melody coherent
```

Use `PASS / BORDERLINE / FAIL`. Keep `C` positive: `PASS` means no audible
step-8 artifact.

## Expected behavior

On screen, entering the surface shows approximately:

```text
F08 LISTEN
TEST ONLY                         01/11
DnB 5 B  Melodic
MINOR FALL
OLD 0000    NEW 8080
> OLD   BPM ...   CHANGED
ADDED
A progression   B movement natural
C no-step8      D roles coherent
```

`A`/`B` immediately restarts the same one-bar loop on the selected side.
`Left`/`Right` changes cases and restarts on OLD. `G` restarts the current side.

Serial prints one line for every playback selection:

```text
[F08-LISTEN] case=1/11 variant=OLD mode=DnB ordinal=5 voice=B ...
```

There must be no ordinary Genre regeneration involved while A/B switching.

## Troubleshooting

### `Ctrl+F` does not enter F08 LISTEN

Confirm you flashed the dedicated review binary from
`build/cardputer-adv-f08-listen`, not the normal GroovePuter build. With Serial
open, pressing `Ctrl+F` should produce a `[KEY]` line with `ctrl=1` and `key=0x66`.
If that line is missing, the issue is below the review UI in the physical keyboard
input path.

### `F08 FIXTURE FAILED`

Rebuild the review binary. The generated fixture header may be missing or the
frozen corpus contract may have moved:

```bash
rm -rf build/f08-listen-generated build/cardputer-adv-f08-listen
bash scripts/build_f08_listen.sh --warnings all
```

### Build stops while generating OLD

That is intentional fail-closed behavior. The generator reverses the exact
known F08 ownership patch only in a temporary source file. If the F08 source
shape changed, update/review the listening harness instead of silently
approximating OLD behavior.

### I hear silence

Exit with `Ctrl+F`, verify the normal firmware audio path works, then enter
again. The page explicitly unmutes all ten GroovePuter tracks during review and
restores their previous mute state on exit.

### My Bank B pattern changed

Expected. Bank B pattern 1 and Song B row 1 are the declared destructive review
sandbox.

## Acceptance checklist

- [ ] Normal repository `src/generation/migration/strong_rhythm_migration.cpp` is unchanged by this review branch.
- [ ] Review build generates 11 OLD and 11 NEW fixtures successfully.
- [ ] Generator proves drums are identical OLD vs NEW for all 11 cases.
- [ ] Generator proves non-pitch SynthStep fields are identical OLD vs NEW.
- [ ] Cases 1–8 change the selected frozen corpus voice.
- [ ] Cases 9, 10 and 11 keep the selected frozen corpus output unchanged.
- [ ] `Ctrl+F` enters and exits the surface.
- [ ] `Left/Right` walks all 11 cases and resets to OLD.
- [ ] `A` plays OLD.
- [ ] `B` plays NEW.
- [ ] `G` replays the current side from the beginning.
- [ ] Previous BPM, transport, Song/pattern selection and mutes return after exit.
- [ ] All 11 listening rows are recorded with A/B/C/D + note.
