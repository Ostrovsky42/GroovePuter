# Step Note Entry — Cardputer ADV acceptance

## Purpose

Validate the optional NOTE ENTRY layer on Synth A/B NOTES with audition-first editing: note keys replace the current step without moving the cursor, and Enter is the only commit-and-advance action.

## Hardware list

- M5Stack Cardputer ADV
- USB-C cable
- headphones or the built-in speaker
- optional SEQTRAK over USB MIDI for an external sound check

## Wiring

No external wiring is required. Use the normal Cardputer ADV power/audio setup. If SEQTRAK is connected, use the existing USB-MIDI route; this test does not change MIDI wiring.

## Build / Flash

```bash
git switch agent/20260808-08-note-entry-audition
python3 tests/test_step_note_entry_source_regressions.py
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
```

## Expected behavior

1. Open `SYNTH A -> NOTES` or `SYNTH B -> NOTES`.
2. Existing controls work unchanged while NOTE ENTRY is off: `Q..I` selects patterns, `A/Z` edits pitch, `S/X` edits octave.
3. Press `N`. Toast: `NOTE ENTRY: ON`.
4. In NOTE ENTRY:
   - `A S D F G H J K L` enter chromatic notes from C3 upward;
   - `Q W E R T Y U I O P` enter chromatic notes from C4 upward;
   - pressing different note keys repeatedly replaces the note in the same focused step so the loop can be auditioned without chasing the cursor;
   - `Enter` moves to the next step and is the only automatic commit-and-advance action;
   - arrow keys move the step cursor left/right and between grid rows without writing notes;
   - `Backspace` clears the focused step without advancing;
   - `;` writes the last entered pitch into the focused step without advancing;
   - holding a note key extends that pitch through following steps using the existing slide/legato chain while the visible cursor stays on the original step;
   - hold extension stops at the end of the pattern instead of wrapping into step 1.
5. Press `N` again. Toast: `NOTE ENTRY: OFF`; legacy bindings immediately return.

## Troubleshooting

- If a held key creates separate retriggers instead of a slide chain, verify Cardputer key repeat is still approximately 350 ms initial delay / 80 ms repeat interval.
- If a quick double-tap of the same key is interpreted as hold, record Serial `[KEY]` timestamps; the first hold-repeat detector expects 250–500 ms and subsequent repeats no more than 180 ms apart.
- If a note key moves the cursor before Enter, the old auto-advance implementation is still running.
- If `Q..I` changes patterns while NOTE ENTRY is on, the NOTE ENTRY input router is not active.
- If notes survive `Backspace`, verify the event reaches the NOTES page rather than global back navigation.

## Acceptance checklist

- [ ] Firmware builds for Cardputer ADV.
- [ ] NOTE ENTRY is off after opening/resetting the page.
- [ ] `N` toggles NOTE ENTRY on/off.
- [ ] Both Synth A and Synth B accept direct notes.
- [ ] Repeated different note keys replace the same focused step without moving the cursor.
- [ ] Enter advances exactly one step and wraps from the last step to step 1.
- [ ] Arrow Left/Right moves between steps without editing notes.
- [ ] Arrow Up/Down moves between grid rows without editing notes.
- [ ] `Backspace` clears only the focused step and does not advance.
- [ ] `;` repeats the last pitch into the focused step and does not advance.
- [ ] Holding one key creates a same-pitch slide/legato chain across following steps without moving the visible cursor.
- [ ] Hold extension does not wrap past the end of the pattern.
- [ ] Fast repeated taps remain usable for audition/replacement.
- [ ] Turning NOTE ENTRY off restores `Q..I`, `A/Z`, `S/X`, `B`, `G`, `F` behavior.
- [ ] Playback continues while entering/holding notes without audio crackle or transport reset.
- [ ] Save/reboot/load preserves the entered notes and slide chain.
