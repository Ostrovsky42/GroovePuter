# Performance Keyboard MONO / POLY acceptance

## Purpose

Verify that Cardputer-Adv PERFORM behaves like a normal external MIDI keyboard for Synth A/B/DX: every physical key keeps its own NoteOn/NoteOff lifecycle, while the receiving synth decides whether those held notes are rendered monophonically or polyphonically.

The Cardputer keyboard must **not** play the internal Synth A/B voices. Internal Synth A/B remain sequencer/pattern instruments.

For the established SEQTRAK profile, GroovePuter requests the receiver voice mode with SEQTRAK CC26 on CH8..10:

- `CC26=0` -> MONO.
- `CC26=1` -> POLY.

We intentionally do not use MIDI Channel Mode CC126/CC127 for this live switch because those messages include All Notes Off semantics and could cut Pattern/SMF notes that share CH8/CH9 with PERFORM.

Plain MONO and POLY therefore send the same physical key events. Their **audible difference is at the receiver**:

| Input | MONO | POLY |
| --- | --- | --- |
| `A down` | NoteOn A; receiver sounds one mono voice | NoteOn A; A sounds |
| `S down` while A held | NoteOn S; receiver applies its mono priority/legato | NoteOn S; A + S can sound |
| `S up` while A held | NoteOff S; receiver may fall back to still-held A without a new A NoteOn from GroovePuter | NoteOff S; A continues |
| `A up` | NoteOff A | NoteOff A |
| Voice allocation | receiver MONO | receiver POLY |
| GroovePuter synthetic restore NoteOn | **never** | **never** |

Generated CHORD/ARP/Strum/Ratchet/Euclidean output requests POLY receiver mode so multi-note GroovePuter material is not collapsed by a receiver left in MONO.

## Hardware list

- M5Stack Cardputer-Adv.
- Yamaha SEQTRAK connected as the primary USB-MIDI receiver.
- Existing GroovePuter -> SEQTRAK USB cable/adapter.
- Optional second class-compliant MIDI receiver for confirming that unsupported SEQTRAK CC26 fails open and notes still transmit.

## Wiring

No GPIO, I2C or audio wiring changes are required.

Use the existing GroovePuter USB-MIDI connection. Performance targets remain:

- Synth A -> MIDI CH8.
- Synth B -> MIDI CH9.
- DX -> MIDI CH10.
- Drums -> native independent MIDI CH1..7 lanes.

## Velocity

Cardputer keys do not provide pressure/velocity sensing, so PERFORM uses one fixed runtime velocity for future key presses.

- Default: `100`.
- Minimum: `10`.
- Maximum: `120`.
- Step: `10`.
- Open `Tab -> PERFORMANCE TOOLS`.
- `-` decreases velocity by 10.
- `=` / `+` increases velocity by 10.
- Changing velocity does not retrigger or rewrite notes that are already held.
- Outside the tools overlay, `-` and `=` keep their existing octave controls.

## Polyphony limits

- Plain manual key table: up to 19 held performance keys in software (`A..L` plus `Q..P`). Hardware rollover can be lower for particular combinations.
- `CHORD MAJ`, `MIN`, `5TH`: 3 generated notes per root.
- `CHORD MIN7`: 4 generated notes per root.
- `CHORD MEMORY`: up to 8 generated notes per root.
- Direct `POLY+CHORD`: maximum 16 unique simultaneous generated MIDI notes across all held roots.
- Overlapping chord tones count once and remain active until the final root that needs them is released.
- MIDI note range remains 12..95.
- `ARP+CHORD` uses a bounded 16-note pool and emits one arpeggiated note at a time.
- Ratchet/Euclidean keep the existing bounded step scheduler.

## Build / Flash

From the repository root:

```bash
./tests/run_host_tests.sh
./scripts/build.sh
./scripts/build_seqtrak_midi_only.sh
```

Flash with the normal Cardputer-Adv upload procedure and open Serial Monitor with the project's normal baud rate.

## Expected behavior

1. Open `PERFORM`, NOTE mode on, Synth A selected.
2. Open `Tab -> PERFORMANCE TOOLS`. `9 VOICE MONO` is the default and velocity shows `100`.
3. In MONO perform `A down -> S down -> S up -> A up`. The receiver may use its own last/high/low priority, but GroovePuter must transmit the physical sequence `NoteOn A -> NoteOn S -> NoteOff S -> NoteOff A`. There must be no second `NoteOn A` after `S up`.
4. Repeat on Synth B and DX. Internal Synth A/B must remain silent from PERFORM.
5. Press `9` for POLY. Repeat the same physical sequence. A and S may sound together; releasing S leaves A sounding without a new attack from GroovePuter.
6. In tools, press `-`: velocity becomes 90. Press a new note and confirm velocity 90. Press `+`: it returns to 100. Repeated presses clamp at 10 and 120.
7. Change velocity while holding a note. The held note must not retrigger; only later NoteOn events use the new value.
8. Enable `CHORD 5TH` in POLY. Hold `A`, then `Q`. The shared tone must not retrigger and must remain until the last root that owns it is released.
9. Direct POLY+CHORD must not exceed 16 unique simultaneous generated notes.
10. Change voice mode, target, NOTE mode, or press `X` Panic with notes held. No stuck notes may remain.
11. While Pattern/SMF owns CH8/CH9, changing PERFORM MONO/POLY must not issue CC126/127 or globally clear that channel's other owners.
12. Drums remain unchanged on CH1..7.

## Troubleshooting

- A held A attacks again after `A down -> S down -> S up`: regression. Capture the MIDI monitor sequence; GroovePuter must not generate a restoration NoteOn.
- MONO still sounds polyphonic on a non-SEQTRAK receiver: expected if that receiver ignores Yamaha CC26. Set its mono mode manually; note transmission should still work.
- SEQTRAK does not switch MONO/POLY: verify the target is CH8, CH9 or CH10 and capture CC26 plus NoteOn/NoteOff traffic.
- Pattern/SMF note is cut when voice mode changes: regression. Confirm no CC126/127 was transmitted; the implementation uses SEQTRAK CC26 specifically to avoid Channel Mode All Notes Off semantics.
- Velocity does not change: confirm `PERFORMANCE TOOLS` is open before pressing `-` or `=`. Outside the overlay those keys change octave.
- Velocity goes below 10 or above 120: regression.
- Internal Synth A/B sounds from PERFORM: regression; PERFORM is external-MIDI-only.
- A note remains stuck: press `X` Panic and capture target/channel/key sequence.

## Acceptance checklist

- [ ] MONO is the default voice mode.
- [ ] Plain MONO sends every physical NoteOn/NoteOff; no controller-enforced one-note replacement remains.
- [ ] `A down -> S down -> S up -> A up` contains no synthetic second NoteOn A.
- [ ] SEQTRAK MONO requests `CC26=0` on the selected CH8..10 target.
- [ ] `Tab -> 9` toggles to POLY and SEQTRAK requests `CC26=1`.
- [ ] POLY allows multiple held notes to sound together on a poly receiver.
- [ ] Releasing any plain POLY key stops only that key and never retriggers another held key.
- [ ] Generated performance tools request POLY receiver mode.
- [ ] No CC126/CC127 live mode switching is used on shared Pattern/SMF channels.
- [ ] Velocity defaults to 100.
- [ ] Tools `-` / `+` change velocity exactly by 10.
- [ ] Velocity clamps at 10 and 120.
- [ ] Velocity changes affect future NoteOn only; held notes are untouched.
- [ ] Outside tools, `-` / `=` still control octave.
- [ ] PERFORM Synth A/B never sound internal Synth A/B.
- [ ] Synth A -> CH8, Synth B -> CH9, DX -> CH10 remain unchanged.
- [ ] POLY+CHORD keeps overlapping tones without retrigger and never exceeds 16 unique notes.
- [ ] Mode/target/NOTE/Panic cleanup leaves no stuck notes.
- [ ] Pattern/sequencer playback remains unchanged.
- [ ] Drums CH1..7 remain unchanged.
- [ ] Host feature tests pass apart from explicitly documented base failures.
- [ ] Cardputer-Adv normal build passes.
- [ ] SEQTRAK MIDI-only build passes.
