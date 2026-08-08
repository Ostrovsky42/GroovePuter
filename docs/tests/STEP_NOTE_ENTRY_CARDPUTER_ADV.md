# Step Note Entry — Cardputer ADV acceptance

## Purpose

Validate the optional fast note-entry layer on Synth A/B NOTES without changing the legacy editor when the mode is off.

## Hardware list

- M5Stack Cardputer ADV
- USB-C cable
- headphones or the built-in speaker
- optional SEQTRAK over USB MIDI for an external sound check

## Wiring

No external wiring is required. Use the normal Cardputer ADV power/audio setup. If SEQTRAK is connected, use the existing USB-MIDI route; this test does not change MIDI wiring.

## Build / Flash

```bash
git switch agent/20260808-06-step-note-entry
python3 tests/test_step_note_entry_source_regressions.py
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
```

## Expected behavior

1. Open `SYNTH A -> NOTES` or `SYNTH B -> NOTES`.
2. Existing controls work unchanged while NOTE ENTRY is off: `Q..I` selects patterns, `A/Z` edits pitch, `S/X` edits octave.
3. Press `N`. Toast: `NOTE ENTRY: ON`.
4. In NOTE ENTRY:
   - `A S D F G H J K L` enter chromatic notes from C3 upward.
   - `Q W E R T Y U I O P` enter chromatic notes from C4 upward.
   - every new note writes to the focused step and advances the cursor by one step;
   - `Backspace` clears the focused step without advancing;
   - `;` repeats the last entered pitch and advances;
   - holding a note key extends the same pitch through following steps using the existing slide/legato chain.
5. Press `N` again. Toast: `NOTE ENTRY: OFF`; legacy bindings immediately return.

## Troubleshooting

- If a held key creates separate retriggers instead of a slide chain, verify Cardputer key repeat is still approximately 350 ms initial delay / 80 ms repeat interval.
- If a quick double-tap of the same key is interpreted as hold, record Serial `[KEY]` timestamps; the first hold-repeat detector expects 250–500 ms and subsequent repeats no more than 180 ms apart.
- If `Q..I` changes patterns while NOTE ENTRY is on, the new input router is not active.
- If notes survive `Backspace`, verify the event reaches the NOTES page rather than global back navigation.

## Acceptance checklist

- [ ] Firmware builds for Cardputer ADV.
- [ ] NOTE ENTRY is off after opening/resetting the page.
- [ ] `N` toggles NOTE ENTRY on/off.
- [ ] Both Synth A and Synth B accept direct notes.
- [ ] A new note advances exactly one step.
- [ ] Cursor wraps from the last step to step 1.
- [ ] `Backspace` clears only the focused step.
- [ ] `;` repeats the last pitch.
- [ ] Holding one key creates a same-pitch slide/legato chain across following steps.
- [ ] Fast repeated taps still create separate notes.
- [ ] Turning NOTE ENTRY off restores `Q..I`, `A/Z`, `S/X`, `B`, `G`, `F` behavior.
- [ ] Playback continues while entering/holding notes without audio crackle or transport reset.
- [ ] Save/reboot/load preserves the entered notes and slide chain.
