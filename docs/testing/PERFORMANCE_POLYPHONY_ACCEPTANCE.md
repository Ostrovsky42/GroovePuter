# Performance Keyboard MONO / POLY acceptance

## Purpose

Verify that Cardputer-Adv PERFORM is an external-MIDI keyboard for Synth A/B/DX and supports both monophonic and true per-key polyphonic output.

The Cardputer keyboard must **not** play the internal Synth A/B voices. Internal Synth A/B remain sequencer/pattern instruments.

`MONO` is the compatibility/default external MIDI mode with last-note priority.

`POLY` is manual external-MIDI polyphony: every held performance key owns an independent NoteOn/NoteOff on the selected external synth channel. Direct `POLY+CHORD` treats all held keys as chord roots and sustains the union of their generated notes: adding or releasing another root changes only the notes that actually enter or leave that union. Unchanged held notes must never be retriggered. ARP, Ratchet and Euclidean retain their existing step-generated behavior and are also external-MIDI-only from PERFORM.

## Hardware list

- M5Stack Cardputer-Adv.
- Yamaha SEQTRAK or another USB-MIDI receiver that can play chords on one MIDI channel.
- The same USB cable/adapter already used for GroovePuter -> SEQTRAK MIDI testing.

## Wiring

No GPIO, I2C or audio wiring changes are required.

Use the existing GroovePuter USB-MIDI connection to the external receiver. Performance targets retain the existing channel map:

- Synth A -> MIDI CH8.
- Synth B -> MIDI CH9.
- DX -> MIDI CH10.
- Drums -> native independent MIDI CH1..7 lanes.

## Polyphony limits

The limits are explicit and fixed-size:

- Plain manual `POLY`: up to 19 held performance keys in software (`A..L` plus `Q..P`). Physical keyboard rollover can be lower for some key combinations and must be checked on hardware.
- `CHORD MAJ`, `MIN`, `5TH`: 3 generated notes per root.
- `CHORD MIN7`: 4 generated notes per root.
- `CHORD MEMORY`: up to 8 generated notes per root.
- Direct `POLY+CHORD`: maximum 16 **unique simultaneous generated MIDI notes across all held roots**. Duplicate/overlapping chord tones count once and remain sounding until the last held root that needs them is released.
- MIDI note range remains 12..95; chord tones that clamp to the same upper/lower boundary are deduplicated.
- `ARP+CHORD` uses the same bounded 16-note union as its pool but emits one arpeggiated note at a time.
- Ratchet/Euclidean step scheduling retains its existing bounded scheduler contract rather than multiplying every held root into a 16-note simultaneous step.

## Build / Flash

From the repository root:

```bash
./tests/run_host_tests.sh
./scripts/build.sh
```

For the SEQTRAK MIDI-only profile, also run:

```bash
./scripts/build_seqtrak_midi_only.sh
```

Flash with the project's normal Cardputer-Adv upload command/script.

## Expected behavior

1. Open `PERFORM` and keep NOTE mode enabled.
2. Press `Tab` to open `PERFORMANCE TOOLS`.
3. Tool `9` shows `VOICE MONO` by default and the UI marks the keyboard as `EXT MIDI ONLY`.
4. Select Synth A. In `MONO`, hold `A`, then press `S`: the external receiver uses last-note priority; releasing `S` restores `A`.
5. During step 4 the Cardputer internal Synth A must stay silent. Repeat on Synth B; its internal voice must also stay silent.
6. Press `9`. The toast shows `VOICE: POLY / EXT MIDI`.
7. Select Synth A, Synth B or DX and hold two or three performance keys at once. The external receiver must sound all held notes together.
8. Release only the middle key. Only that note must stop; the other held notes must continue. The first held note must **not** receive a second NoteOn.
9. Enable `CHORD 5TH` in POLY. Hold `A` (C-root chord), then `Q` (upper C-root chord). The shared C tone must sound continuously; pressing/releasing either root must not retrigger or prematurely stop the shared tone.
10. In POLY+CHORD, hold several roots. The total simultaneous generated set must never exceed 16 unique MIDI notes.
11. Release the remaining keys in any order. Every note that leaves the held-root union must receive its matching NoteOff; no stuck note may remain.
12. Hold multiple notes and change `POLY -> MONO`, change target, disable NOTE mode, or press `X` Panic. All external POLY notes must stop.
13. Enable ARP/Chord/Strum/Ratchet/Euclidean on Synth A/B. Generated notes must go to external MIDI only; the internal Synth A/B voices must remain unaffected.
14. Drums remain independent native lanes and are not changed by MONO/POLY.

## Troubleshooting

- If the internal Synth A/B sounds when pressing PERFORM keys, this is an acceptance failure. The PERFORM keyboard must be external-MIDI-only.
- If only one external note sounds in plain `POLY`, confirm the PERFORM chip says `POLY` and that the external receiver/patch is itself polyphonic.
- If a previously held note audibly attacks again when another POLY key or POLY+CHORD root is released, this is an acceptance failure. Capture the exact keys/chord mode and target channel.
- If a shared POLY+CHORD tone stops when only one of two roots is released, this is an ownership failure; press `X` Panic and report the root keys and chord mode.
- If more than 16 unique chord tones are requested, the direct POLY+CHORD set is intentionally bounded to 16; additional unique tones are not added until capacity becomes available.
- If no external note sounds, confirm USB MIDI readiness and the selected target channel before debugging voice mode.
- If a note remains stuck after release, press `X` Panic and capture serial output plus the exact target/channel and key sequence. This is an acceptance failure.
- If `9` changes a global command instead of voice mode, confirm the `PERFORMANCE TOOLS` overlay is open with `Tab`.
- If USB MIDI is not ready, resolve the existing endpoint/cable/SEQTRAK routing issue first; MONO/POLY does not change USB enumeration or wiring.

## Acceptance checklist

- [ ] `MONO` is the boot/default voice mode.
- [ ] MONO external last-note-priority behavior is unchanged.
- [ ] `Tab -> 9` toggles `MONO <-> POLY` without leaving PERFORM.
- [ ] UI marks both modes as external MIDI only; no `INT+USB` label remains.
- [ ] PERFORM Synth A keys never sound internal Synth A.
- [ ] PERFORM Synth B keys never sound internal Synth B.
- [ ] PERFORM ARP/Chord/Strum/Ratchet/Euclidean never sound internal Synth A/B.
- [ ] Two simultaneous keys produce two simultaneous external notes in POLY.
- [ ] Three simultaneous keys produce three simultaneous external notes in POLY.
- [ ] Releasing a non-last/middle key stops only that exact note.
- [ ] Releasing one POLY key does not retrigger any still-held plain POLY note.
- [ ] POLY+CHORD keeps unchanged chord tones sounding without a second NoteOn.
- [ ] Overlapping chord tones remain active until the final root that needs them is released.
- [ ] Direct POLY+CHORD never exceeds 16 unique simultaneous generated notes.
- [ ] Matrix reconciliation/key-up recovery does not retrigger unrelated held notes.
- [ ] Synth A -> CH8, Synth B -> CH9, DX -> CH10 remain unchanged.
- [ ] Target change, mode change, NOTE off and Panic clean all POLY notes.
- [ ] Pattern/sequencer playback of internal Synth A/B is unchanged.
- [ ] Drums CH1..7 behavior is unchanged.
- [ ] No stuck MIDI notes after rapid multi-key press/release sequences.
- [ ] Host tests pass, except any explicitly documented pre-existing base failure.
- [ ] Cardputer-Adv normal build passes.
- [ ] SEQTRAK MIDI-only build passes.
