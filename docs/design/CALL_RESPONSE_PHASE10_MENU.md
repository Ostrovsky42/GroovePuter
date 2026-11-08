# CALL/RESPONSE phase 10: PARTNER GENRE and LENGTH hidden from the menu

Status: implemented, host tests green, Cardputer build PASS (DRAM 185796, unchanged). Not hardware-tested.

## Decision

PERFORM > INPUT no longer shows the PARTNER rows **GENRE** and **LENGTH**. Nothing else changes:

- the partner always answers in the **project genre** (`kGenreAuto`, AUTO);
- every answer note keeps **100 %** of its length (`AnswerLength::Full`).

These were already the defaults. The values were never saved (session only), so after a reboot they were
AUTO and 100 % anyway; with the rows gone they cannot be changed from the screen.

## Why

- **GENRE**: an explicit genre (or NONE) shaded the answer's style (ending colour and the like). On the
  device the difference was hard to hear against AUTO, and AUTO (the project genre) is what a player wants almost every time.
- **LENGTH** (phase 9): it was added to make legato answers staccato, but in practice it was not used.
  The reply already follows the CALL's own articulation.

Two rows that do little made the PARTNER block longer than the screen (it scrolled 16 rows in a 6-row view).

## Menu layout

| Row | Before (phase 9) | After (phase 10) |
|---|---|---|
| 8 | PARTNER | PARTNER |
| 9 | BARS | BARS |
| 10 | GENRE | CHANNEL |
| 11 | CHANNEL | HOLD |
| 12 | LENGTH | EVOLVE |
| 13 | HOLD | REPLACE |
| 14 | EVOLVE | — |
| 15 | REPLACE | — |

INPUT has 14 rows (0..13). UP/DOWN wrap inside them; `</>` and ENTER act only on the row shown.
The restored view row (`kMaxRowByContext` in `perform_page.h`) is 13 for INPUT; a stale row above that
falls back to row 0. The hints of the two removed rows are removed; the other hints are unchanged.

## What was kept

The listener API is untouched and still tested: `setAnswerGenreSetting()`, `answerGenreSetting()`,
`cycleAnswerGenre()`, `answerGenre()`, `setAnswerLength()`, `answerLength()`, `answerLengthPercent()`,
`cycleAnswerLength()` (`tests/test_cr_partner_settings.cpp`, `tests/test_cr_phase9_length.cpp`). The answer
path still reads them, so audio, NoteOff and cancel paths are unchanged.

## Files

- `src/ui/pages/perform_page.cpp`: row count 16 -> 14, rows renumbered in `adjustSelectedValue()`,
  `toggleSelectedValue()`, `cyclePartnerSetting()`, `selectedRowHint()` and the INPUT drawing.
- `src/ui/pages/perform_page.h`: `kMaxRowByContext` INPUT 15 -> 13.
- `tests/test_cr_phase10_menu.cpp` (target `cr_phase10` in `tests/run_smart_play_tests.sh`): the page with
  the SDL host source set; DOWN visits exactly 0..13, no GENRE/LENGTH label at any scroll position,
  BARS/CHANNEL/EVOLVE/REPLACE act on rows 9/10/12/13, `</>`/ENTER on rows 9..13 leave AUTO and 100 %,
  view continuity keeps row 13 and drops 14/15.

## How to bring them back

1. In `rowCountForContext()` return the larger count for `Input`, and raise `kMaxRowByContext[4]`.
2. Insert the rows back into the INPUT drawing (the GENRE value formatting is in git history before phase 10:
   `AUTO <project genre>` / `NONE` / genre name; LENGTH is `answerLengthPercent()` with `%`).
3. Add their `case` to `cyclePartnerSetting()` (`cycleAnswerGenre()`, `cycleAnswerLength()`), to the
   `adjustSelectedValue()` / `toggleSelectedValue()` partner ranges and to `selectedRowHint()`
   (`"</> AUTO=PROJECT  NONE=PLAIN  OR GENRE"`, `"</> ANSWER LENGTH: 100/75/50/25 %"`), renumbering the rows below.
4. Update `tests/test_cr_phase10_menu.cpp` (row count, labels, row numbers).
