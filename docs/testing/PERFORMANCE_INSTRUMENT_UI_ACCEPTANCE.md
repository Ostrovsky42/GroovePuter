# 0.9.9 Performance Instrument UI — Cardputer ADV acceptance

## Purpose

Hardware-review the visible UI slice stacked on Performance Instrument V1.

Expected entry path:

```text
PERFORM
  Tab
   -> KEY | CHORD | ARP | RHYTHM
```

This checkpoint changes presentation and command routing only. `PerformanceKeyboard` remains the musical/MIDI owner. Page-local state is limited to the selected context and selected row.

## Hardware list

- M5Stack Cardputer ADV (ESP32-S3, 240x135 display)
- USB-C data cable for power/flashing/serial
- Optional existing MIDI receiver used for GroovePuter hardware review

## Wiring

No new wiring is introduced by this checkpoint.

Use the same USB/DIN MIDI connection already validated for the parent Performance Instrument branch. Do not add a second MIDI output path for this test.

## Build / flash

Checkout the exact branch:

```bash
git checkout feature/20260903-02-0.9.9-performance-instrument-ui
git rev-parse HEAD
```

Run the focused UI source gate:

```bash
python3 tests/test_performance_instrument_ui_source_regressions.py
```

Run the parent PerformanceKeyboard/MIDI regressions separately and record any inherited #423 failures rather than hiding them as UI results.

Build and flash with the repository's normal Cardputer ADV target. Keep Serial open during the hardware run and record reset/watchdog/panic output if any appears.

## Controls

### Live PERFORM

Existing live bindings remain unchanged:

```text
Tab          Open Performance Instrument tools at KEY
N            NOTE mode
\            Output target
, / .        Scale -/+
- / =        Octave -/+
X            Panic current live target
```

### Performance Instrument tools

```text
Tab          Next context KEY -> CHORD -> ARP -> RHYTHM
Backspace    Previous context (the Cardputer has no Shift key)
Up/Down      Previous/next parameter row
Left/Right   Decrease/increase selected value
Enter        Toggle or secondary action for selected row
Esc / `      Return to live PERFORM
- / =        Keep their live meaning (octave down/up)
```

The `; , . /` characters are swallowed while the layer is open. On the Cardputer
those keys are the arrow keycaps and the firmware dispatches both the arrow
scancode and the printed character; without this, every arrow press would also
change the scale through the live `, / .` bindings. Arrows are accepted with Fn
held as well, because the keycaps label them as Fn alternates.

Layout of the tools surface:

```text
line 0   [KEY] CHORD ARP RHYTHM            SYN A MONO
line 1-6 one parameter per row; selected row is a filled bar with ">"
line 7   hint for the selected row, e.g. "</> PULSES  ENTER LENGTH"
```

The tab bar uses fixed slots, so labels never shift when the context changes.
Each context remembers its own selected row. Opening the layer shows no toast.
The whole surface is repainted every frame because `MiniAcidDisplay::update()`
clears the screen before every page draw; a partial repaint is not possible here.

Output target and receiver MONO/POLY are visible rows in KEY (`OUTPUT`, `VOICE`), not hidden shortcuts. `9` remains as a compatibility alias and still dispatches to the authoritative `PerformanceKeyboard::toggleVoiceMode()` transition. That engine transition may clean sounding notes by design; the UI does not snapshot or synthetically replay held physical keys afterward.

Context-specific Enter actions:

```text
KEY OUTPUT      Next output target (SYNTH A / SYNTH B / DX / DRUMS)
KEY VOICE       MONO/POLY receiver toggle
CHORD SPREAD    CLOSE/WIDE toggle
CHORD LEADING   OFF/NEAREST toggle when available
CHORD MEMORY    Capture held 2+ notes, otherwise clear existing memory
ARP ARP         OFF/ON toggle
ARP LATCH       Toggle when ARP is enabled
RHYTHM EUCLID   Cycle Euclidean length
RHYTHM STRUM    Cycle strum direction when audible
```

## Expected behavior

### KEY

Visible authoritative values:

```text
ROOT       C
SCALE      DORIAN
OCTAVE     +1
VELOCITY   100
OUTPUT     SYNTH A  CH8
VOICE      MONO
```

`OUTPUT` and `VOICE` call the same engine transitions as `\` and `9`; both may stop sounding notes by design, and the hint line says so.

Changing ROOT, SCALE, or OCTAVE updates the configuration shown on screen. A physical note that was already held keeps its resolved note identity until physical key-up. The next physical key-down uses the new configuration.

### CHORD

Visible rows:

```text
MODE
INVERSION
SPREAD
LEADING
MEMORY
```

Fixed and scale-degree modes remain distinguishable (`MAJ`, `MIN`, `5TH`, `SUS2`, `SUS4`, `7`, `MAJ7`, `MIN7`, `SCALE3`, `SCALE7`, `MEM`). Inactive voicing controls display `N/A` rather than a fake editable value.

### ARP

Visible rows:

```text
ARP
ORDER
OCTAVES
LATCH
```

`ARP` distinguishes `OFF`, `ON`, and `LATCHED`. `AS PLAYED` is displayed literally. ARP-only controls display `N/A` while ARP is off.

RATE and GATE are not ARP-only: they drive the shared step clock that ARP, RATCHET and EUCLID all use, so they live in RHYTHM and stay editable with ARP off. Previously they were hidden behind `N/A` whenever ARP was off, which made RATCHET and EUCLID timing impossible to set.

### RHYTHM

Visible rows:

```text
RATE       1/16
GATE       60%
RATCHET    x2
EUCLID     5/16
ROTATE     3
STRUM      UP 24ms
```

Rates are musician-facing values: `1/4`, `1/8`, `1/8T`, `1/16`, `1/16T`, `1/32`. When a requested rate is waiting for the next coherent pulse boundary, the value is marked `NEXT`; with no note held there is no clock, so `NEXT` stays until the next played note commits it. `RATCHET` shows `OFF` for x1, `EUCLID` shows `OFF  LEN n` for zero pulses (every step fires), and `STRUM` shows `OFF` for 0 ms. Enabling ARP or a ratchet clears strum, and a strum clears ratchet; the hint line names these side effects.

Rotation is `N/A` for effectively all-on/all-off Euclidean states. Strum is `N/A` when the current authoritative configuration cannot use chord strumming (for example ARP enabled or chord mode off).

The parent seeded-mutation foundation is not promoted to a generic `CHAOS` control in this checkpoint. No new mutation semantics are invented.

## Hardware procedure

1. Boot firmware and confirm no reset/watchdog loop.
2. Open `PERFORM`; confirm baseline physical keyboard input still works.
3. Press `Tab`; confirm `KEY | CHORD | ARP | RHYTHM` is immediately visible and KEY is structurally selected with brackets.
4. In KEY, verify ROOT/SCALE/OCTAVE/VELOCITY/OUTPUT/VOICE and change each with Left/Right. Confirm that Left/Right/Up/Down do not change SCALE as a side effect (the `, .` keycaps).
5. Hold one physical note; change ROOT, SCALE, and OCTAVE. Confirm the held note is not retriggered/reinterpreted; release it, then press a new note and confirm the new configuration applies.
6. Open CHORD and cycle all supported modes. Verify display and MIDI/audible result agree. Check inversion, spread, leading, and memory.
7. Open ARP and verify ORDER, OCTAVES, and LATCH.
8. Open RHYTHM and verify RATE (including `1/8T` and `1/16T`), GATE, RATCHET, EUCLID, ROTATE, STRUM, and the expected `N/A` dependency states. Confirm RATE/GATE apply to RATCHET and EUCLID with ARP off.
9. With transport running, switch repeatedly KEY -> CHORD -> ARP -> RHYTHM. Confirm no pause, stuck note, UI lag, reset, or watchdog.
10. Release every physical key, return to live PERFORM, press `X` Panic, and confirm no note remains sounding.

## Troubleshooting

- **Tab still shows numbered PERFORMANCE TOOLS:** wrong firmware/branch was flashed. Verify the exact HEAD before flashing.
- **Row labels or the tab bar vanish after the first frame / flash on Tab:** the page is partially repainting while the display clears the screen every update. `drawToolsLayer` must clear and redraw everything each frame.
- **A held note changes pitch after ROOT/SCALE/OCTAVE edit:** fail the checkpoint. The UI must not synthesize key-up/key-down or rebuild physical held ownership.
- **RATE changes partway through a pulse:** fail the checkpoint. UI must use the existing pending/NEXT_STEP command path.
- **ROTATE or STRUM changes while shown as N/A:** fail the UI dependency contract.
- **`9` receiver-mode toggle releases notes:** distinguish this from ROOT/SCALE/OCTAVE. `9` invokes the parent engine-owned voice-mode transition; the UI must not restore those notes synthetically.
- **CI host test fails at a known parent assertion:** compare against exact parent `be3e337839bec4eaaef783f856083bd49ec24c16` and report it as an inherited #423 blocker. Do not patch PerformanceKeyboard semantics in this UI checkpoint solely to turn the matrix green.

## Acceptance checklist

### Visual

- [ ] Tab surface is visibly redesigned
- [ ] KEY / CHORD / ARP / RHYTHM are immediately understandable
- [ ] Active context has brackets/structural selection, not color only
- [ ] Selected row and current value are readable on 240x135
- [ ] Long scale/chord labels do not corrupt layout
- [ ] Inactive/no-op controls show N/A
- [ ] Bottom control hints match physical controls
- [ ] Tab bar, row labels, and cursor stay visible on every frame (no one-frame flash after Tab/Left/Right)
- [ ] Hint line explains Left/Right and `Enter` for the selected row and stays above the HUD strip
- [ ] No visible excessive flicker

### Functional

- [ ] Displayed musical values come from PerformanceKeyboard
- [ ] Edits dispatch through PerformanceKeyboard commands/setters
- [ ] ROOT/SCALE/OCTAVE preserve already-held physical note identity
- [ ] AS PLAYED stays AS PLAYED
- [ ] Clocked edits preserve NEXT_STEP coherence
- [ ] No duplicate NoteOn owner
- [ ] No stuck NoteOff
- [ ] Panic semantics remain owned by the engine/output path

### Build

- [ ] Focused UI source gate PASS
- [ ] Existing PerformanceKeyboard results recorded
- [ ] Full host result recorded
- [ ] SDL build result recorded
- [ ] Cardputer ADV build result recorded
- [ ] Warnings result recorded
- [ ] Static DRAM delta recorded
- [ ] Relevant MIDI/SEQTRAK result recorded

### Hardware

- [ ] New Tab UI visually confirmed
- [ ] KEY controls confirmed
- [ ] CHORD confirmed
- [ ] ARP confirmed
- [ ] RHYTHM confirmed
- [ ] Held-note test PASS
- [ ] Transport-running UI test PASS
- [ ] Panic/stuck-note test PASS
