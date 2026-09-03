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
Tab          Cycle KEY -> CHORD -> ARP -> RHYTHM
Left/Right   Previous/next context
Up/Down      Previous/next parameter row
- / +        Decrease/increase selected value
Enter        Toggle or secondary action for selected row
Esc / `      Return to live PERFORM
9            Compatibility receiver MONO/POLY toggle
```

`9` still dispatches to the authoritative `PerformanceKeyboard::toggleVoiceMode()` transition. That engine transition may clean sounding notes by design; the UI does not snapshot or synthetically replay held physical keys afterward.

Context-specific Enter actions:

```text
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
```

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
RATE
OCTAVES
GATE
LATCH
```

`ARP` distinguishes `OFF`, `ON`, and `LATCHED`. `AS PLAYED` is displayed literally. Rates are musician-facing values: `1/4`, `1/8`, `1/8T`, `1/16`, `1/16T`, `1/32`. When a requested rate is waiting for the next coherent pulse boundary, the value is marked `NEXT`.

ARP-only controls display `N/A` while ARP is off.

### RHYTHM

Visible rows:

```text
RATCHET    x2
EUCLID     5/16
ROTATE     3
STRUM      UP 24ms
```

Rotation is `N/A` for effectively all-on/all-off Euclidean states. Strum is `N/A` when the current authoritative configuration cannot use chord strumming (for example ARP enabled or chord mode off).

The parent seeded-mutation foundation is not promoted to a generic `CHAOS` control in this checkpoint. No new mutation semantics are invented.

## Hardware procedure

1. Boot firmware and confirm no reset/watchdog loop.
2. Open `PERFORM`; confirm baseline physical keyboard input still works.
3. Press `Tab`; confirm `KEY | CHORD | ARP | RHYTHM` is immediately visible and KEY is structurally selected with brackets.
4. In KEY, verify ROOT/SCALE/OCTAVE/VELOCITY and change each with `-/+`.
5. Hold one physical note; change ROOT, SCALE, and OCTAVE. Confirm the held note is not retriggered/reinterpreted; release it, then press a new note and confirm the new configuration applies.
6. Open CHORD and cycle all supported modes. Verify display and MIDI/audible result agree. Check inversion, spread, leading, and memory.
7. Open ARP and verify ORDER, RATE, OCTAVES, GATE, and LATCH, including `1/8T` and `1/16T`.
8. Open RHYTHM and verify RATCHET, EUCLID, ROTATE, STRUM, and the expected `N/A` dependency states.
9. With transport running, switch repeatedly KEY -> CHORD -> ARP -> RHYTHM. Confirm no pause, stuck note, UI lag, reset, or watchdog.
10. Release every physical key, return to live PERFORM, press `X` Panic, and confirm no note remains sounding.

## Troubleshooting

- **Tab still shows numbered PERFORMANCE TOOLS:** wrong firmware/branch was flashed. Verify the exact HEAD before flashing.
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
