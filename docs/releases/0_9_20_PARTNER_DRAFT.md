# GroovePuter 0.9.20 — PARTNER

## STATUS

```text
STATUS:
RELEASE CANDIDATE
DEVICE CHECK: OWNER-REPORTED (MIDI-only build, not re-run by the release author)
PUBLICATION: NOT DONE (tag and GitHub Release wait for owner approval)

COMMIT:
fec3237c "fix(pattern): copy follows the selection"

PROFILE:
MIDI-only Cardputer build (CDCOnBoot=default), dynamic FatFs (FS1B)

DRAM:
186236 / 191488 bytes
```

---

## 1. WHAT IS IN 0.9.20

### Partner (CALL / RESPONSE) on PERFORM
- Capture of a two-bar phrase (or four bars) from the keyboard or MIDI IN.
- Answer modes: ANSWER, DUET, LOOP. LOOP repeats the answer on the grid and
  replaces the loop only when a phrase has enough notes (REPLACE 3+ by default).
- Answer goes to the chosen MIDI channel (DX route by default), USB and DIN.
- Chord-aware endings: odd answers end on the fifth or a chord tone, even answers
  on the tonic; genre templates supply the chords and final tones.
- Answer length 100 / 75 / 50 / 25 % (LENGTH row hidden from the menu, default 100 %).
- Settings in PERFORM > INPUT: PARTNER, BARS, CHANNEL, HOLD, EVOLVE, REPLACE.
- `X` on PERFORM silences the partner (mode is kept) and releases live notes.

### Patterns and editing
- Synth steps accept notes from C1 to C7 (24..96). MIDI sends the written note;
  the internal synth plays notes above 71 one or two octaves lower.
- Step settings window `Alt+E` on STEPS and DRUMS: PROB, TIMING, VEL and,
  on drums, FLAM / ROLL. `Enter` or `Esc` closes.
- Copy follows the selection: no selection copies the cursor step, a selected
  cell copies that cell, an area copies the rectangle, `Ctrl+A` copies the whole pattern.

### USB and MIDI keyboard
- Role selection in Project > MIDI > USB Role: Off, Device, Host.
  Host lets an external MIDI keyboard play PERFORM. Needs an OTG cable.

---

## 2. KNOWN LIMITATIONS

- **Stuck note.** A note was reported as stuck once. Logs showed the firmware
  sent matching NoteOff events; the cause is not confirmed. Not fixed in 0.9.20.
- **Silence after a stuck note.** Reported once, not reproduced in the logs.
- **UI frame spike (2.4 s).** Seen once during a blocked-USB window, not reproduced
  afterwards. Not fixed.
- **Genre.** Genre changes the answer ending and the chord templates only; it is
  audibly subtle. Recorded as a known product limit, not a defect.
- **Host role** is not verified with a real OTG keyboard in this release.
- **Import** of MIDI files still folds notes into 24..71.
- **Live play on the internal synth** still clamps to 71 (B4).
- **Standard profile:** the standard Cardputer profile disables sampler, tape and
  waveform according to the external release-prep report. Confirm for this MIDI-only
  build before tagging.

---

## 3. TESTS

Partner and pattern suite (`bash tests/run_smart_play_tests.sh`): exit 0, all
targets pass, including `cr_capture` … `cr_phase10`, `pattern_range_wire`,
`pattern_range_engine`.

Step settings (`bash tests/run_step_settings_tests.sh`): exit 0.

Source regressions that are red on the base commit and are not caused by this
release are listed in the release-prep report and must be triaged before tagging:
`test_pattern_generation_owner_0_9_9_source`, `test_phrase_ui_source_regressions`,
`test_song_playhead_source_regressions`, `test_ui_layout_source_regressions`,
`test_undo_0_9_9_r9_regression_source`, `test_drum_g_undo_0_9_9_source`,
`test_performance_source_regressions`, `test_wavemorph_performance_source_regressions`.

---

## 4. ACCEPTANCE CHECKLIST (before tag)

- [ ] Final image built from `fec3237c` (or the release commit) on a clean tree.
- [ ] Device check on the MIDI-only build: partner ANSWER / LOOP, PARTNER
      silence with `X`, drum step window, copy with `Ctrl+A`, C#5 / E5 from STEPS.
- [ ] Red source-regression tests listed above are fixed or accepted as exceptions.
- [ ] Owner approval for the tag and the GitHub Release.
