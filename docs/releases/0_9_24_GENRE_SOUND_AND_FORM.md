# 0.9.24 — genre sound and genre form

Status: code and host tests done for the parts listed under "Done". Device acceptance:
**not done, waiting for the owner.**

## Done (commits on feature/0.9.24)

| Task | Commit | What |
|---|---|---|
| H2 | 3498a7f1 | Acid, Techno and UK garage choose their idiom variant by the G press (parity), not by salt. Neighbouring presses differ. |
| H12 | bd1d6609 | Base feature snapshot refreshed after H2 (Acid, Techno, UK garage, House change). |
| H3 | 6487975e | FORM has seven levels in Up/Down order: SILENCE, DR, DR+A, DR+B, A+B, +B SPACE, FULL. Codes 5 (A+B) and 6 (DR+B) are new; codes 0..4 keep their meaning. |
| H4 | 33255362 | A source longer than a section walks on through the source. |
| H5 | edd20531 | Six genre forms of 32 bars (House, UK garage, Techno, Acid, Dub, Trip-Hop); other genres keep WAVE. |
| H6 | 7d5e4904 | Opening FORM loads the genre's form when the genre changed (`FORM: <GENRE>`); edits in the same genre are kept. |
| H7 | baf2a7e5 | Sound presets for six genres: CUTOFF, ATTACK, RELEASE, REVERB, DELAY on Synth A (channel 8) and Synth B (channel 9). |
| H8 | 67c8641f | ENTER on the SEQTRAK profile sends the preset, one step per frame, each confirmed before the next; the selected controller is restored. |
| H9 | d08d755c | The D refusal reads `<GENRE> CAN'T GROW: SONG F FORM`; keys.md updated. |

## Not done

- **H1 (LIVELY levels) — blocked.** The planned mapping (CALM = level 2, NORMAL = level 0, LIVELY = level 1)
  was tried and reverted: with the current apply shaping the House counts do not give CALM < NORMAL < LIVELY. Measured (press 1, House):
  bass notes CALM 46, NORMAL 43, LIVELY 35; Synth B onsets CALM 22, NORMAL 41, LIVELY 37. The change was
  reverted; the owner has to decide how the three levels should sound before H1 can land.
- **H10 (listening kit for the forms) — skipped**, because it depends on H1.
- **H8 hint line on GENRE** — the preset hint (for example `A: SHORT BASS  B: PIANO/ORGAN`) is shown only
  in the toast's text if at all; it is not drawn on the GENRE page yet.
- **H11 items beyond this file** — `global_help_content.h` is unchanged; `MANUAL.md` and `keys.md` describe
  FORM as built here.

## Scenario to try on the device

GENRE, Enter (SEQTRAK profile: the sound is sent to the synths) → SONG, `T` (take) → `F` (FORM, opens the
genre's form) → `G` (insert the form rows) → Play. Step M (NEW TAKE + TEMPO) as in the plan. *Not checked
in the code for this release.*

## Acceptance (device) — not done, waiting for the owner

- [ ] GENRE Enter sends the preset to SEQTRAK; the controllers move one by one; the synth page shows the same
      controller it showed before the send.
- [ ] FORM opens with the genre form (`FORM: <GENRE>`); Up/Down walks the seven levels in order.
- [ ] A+B and DR+B sound as their names say; the bass comes off before a drop only with drums.
- [ ] D on Acid or House shows `<GENRE> CAN'T GROW: SONG F FORM`.
- [ ] Acid, Techno and UK garage alternate their variant with each G press.
