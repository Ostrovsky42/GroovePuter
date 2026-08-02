# SMF Channel Inspector Stage

## Purpose

Expose the actual channel structure of a loaded Standard MIDI File before custom routing is introduced. The inspector is read-only and must not change playback timing, route notes, send Program Change, or add another SD scan.

For each of the 16 MIDI channels it reports:

```text
note count
lowest / highest note
average NoteOn velocity
maximum observed polyphony
first Program Change value
likely GM drums on source CH10
current RAW or SEQTRAK-safe destination
```

Metadata is accumulated during the existing load-time stream pass used to build the tempo map. The realtime scheduler, `MidiDispatchTask`, TinyUSB ownership, internal audio, PatternPlayer and MIDI Clock remain unchanged.

Maximum polyphony is the highest number of active NoteOn voices observed on a source channel after balanced NoteOff events are applied. It counts overlapping voices rather than unique pitch classes, so retriggered or layered notes remain visible in the diagnostic.

## Hardware list

- M5Stack Cardputer-Adv / ESP32-S3, PSRAM disabled.
- Yamaha SEQTRAK.
- Data-capable USB-C cable.
- SD card with Format-0 and Format-1 `.mid` files under `/midi`.
- Optional Linux computer with `aseqdump` for routing comparison.

## Wiring

```text
Cardputer-Adv USB-C ---- data USB ---- Yamaha SEQTRAK USB
SEQTRAK audio out -------------------- headphones / speakers
```

PORT.A is not used. If unrelated I2C hardware is attached, Cardputer-Adv PORT.A remains:

```text
SDA GPIO2
SCL GPIO1
Wire
```

## Build / Flash

```bash
bash tests/run_host_tests.sh
bash scripts/build_sdl.sh
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
```

Use the repository-pinned M5Stack dependencies. Do not change the ESP32 core in this stage.

## Expected behavior

Open MIDI Player, load a file, then press `I`:

```text
F1  PPQN 480  TRK 5  USED 8
C01 N0842 C2 -G6  V058 X06 P000 S1
C02 N0310 E1 -C4  V071 X02 P033 S2
C10 N1914 B1 -D#5 V092 X05 P--- DRM
```

Field meanings:

```text
C    source MIDI channel
N    NoteOn count, display capped at 9999
range lowest-highest absolute MIDI note
V    average NoteOn velocity
X    maximum observed simultaneous NoteOn count
P    first Program Change, or ---
S1   SEQTRAK Synth 1 / CH8
S2   SEQTRAK Synth 2 / CH9
DX   SEQTRAK DX / CH10
DRM  GM drum split to native SEQTRAK CH1..7
OFF  safe unmapped destination
RAW  original source channel preserved
```

Controls:

```text
I         inspector / player
Up/Down   scroll used channels
D         performance diagnostics
B         file browser
Space     MIDI Play / Pause remains available
G         GroovePuter Run / Stop remains available
```

Program Change is displayed only. It is not transmitted to SEQTRAK.

## Troubleshooting

### Only drums are listed or audible

Inspect whether the file uses source CH10 for drums and other melodic channels beyond CH1-CH3. In SEQTRAK-safe mode, CH1, CH2 and CH3 map to Synth 1, Synth 2 and DX; CH10 is split into native drums; additional channels remain OFF until Custom Routing is implemented.

### Note range looks too low or high

The inspector shows absolute MIDI notes. This indicates that the future Custom Routing stage may need per-channel transpose or octave controls. SEQTRAK front-panel scale/key settings do not rewrite the MIDI file.

### Program number is visible but sound is different

Expected. This stage does not send Program Change, Bank Select, CC, SysEx or effect settings. SEQTRAK uses the sound currently selected in its project.

### Loading becomes noticeably slower

Treat as a regression. Statistics must be accumulated inside the existing `while (stream_.next(event))` load scan. No second complete stream scan is allowed.

## Acceptance checklist

```text
[ ] normal GroovePuter boot and internal audio are unchanged
[ ] MIDI Player loads the same accepted files
[ ] I opens and closes the channel inspector
[ ] Format-0 file reports the correct PPQN and used channels
[ ] Format-1 file reports the correct track count and used channels
[ ] source CH10 is marked as drums
[ ] note count and min/max range are plausible
[ ] average velocity changes between quiet and loud test files
[ ] chord file reports polyphony greater than one
[ ] Program Change is displayed but not sent
[ ] RAW displays RAW destinations
[ ] SEQTRAK-safe displays S1/S2/DX/DRM/OFF destinations
[ ] Up/Down scrolls without seeking or changing BPM
[ ] Space and G remain usable while inspector is visible
[ ] playback timing and polyphony are unchanged
[ ] no second full SD scan is introduced
[ ] no stuck notes after I/D/B/Space/G/X transitions
[ ] no watchdog, reset or sustained audio underrun
```
