# UI state audit D1 — SYNTH A, MATERIAL BANK, SONG

Status: read-only audit. No source changes made for this document.
Branch base: `b5cf8e5b` with the uncommitted BLACK theme pass (see `docs/releases/UI_BLACK_THEME_UNVERSIONED.md`).

Scope: who owns focus (cursor), selection, and playing for three reference screens, and whether
each visual marker is driven by the right owner. Evidence is code (file:line) unless marked
"runtime".

## 1. Summary

| # | Finding | Screen | Severity | Evidence |
|---|---|---|---|---|
| F1 | BLACK draws the playhead on step 1 while transport is STOPPED | SYNTH A/B | P1 | `pattern_edit_page_legacy.h:1087,1169`; `miniacid_engine.cpp:833` |
| F2 | BLACK playhead ignores which pattern is playing; CYBER checks it | SYNTH A/B | P1 | `pattern_edit_page_legacy.h:1316` (CYBER) vs `1169` (BLACK) |
| F3 | CYBER cursor and playhead share the voice colour; they differ only by border width and a scan line | SYNTH A/B | P2 | `pattern_edit_page_legacy.h:1316–1317, 1323+` |
| F4 | Footer says `[H] HELP`, but plain H is swallowed; help opens on Alt+H | all | P1 | `miniacid_display.cpp:559` (Alt+H), `709–711` (plain H, `return true`) |
| F5 | MATERIAL BANK "current bar" is computed from song position, not from transport state | MATERIAL BANK | P2 | `phrase_page.cpp:220–222, 738, 251, 254–256` |
| F6 | SONG playhead marker is correctly gated on `playing && songMode && slot` | SONG | OK | `song_page.cpp:1947, 2527` |
| F7 | MATERIAL BANK selected bar (outline) and playing bar (fill) use different channels | MATERIAL BANK | OK | `phrase_page.cpp:254–262` |

Priority for the next design step: F1 and F2 (wrong playing indicator), then F4 (misleading prompt).

**Status:** F1 and F2 fixed in the working tree (`pattern_edit_page_legacy.h`, BLACK `drawMinimalStyle`: playhead gated on `isPlaying()` and on the playing pattern, as CYBER does). Runtime check on the SDL harness: STOP shows no playhead on step 1; PLAY shows it on the playing step. F5 fixed in the working tree (`phrase_page.cpp`): fill = playing bar (transport running), accent outline = song position while stopped. F4 open.

## 2. SYNTH A / SYNTH B

### Owners

- Step cursor: `pattern_edit_cursor_` (`pattern_edit_page.h:160`), valid only when `focus_ == Focus::Steps`.
- Pattern row: `pattern_row_cursor_` (slot Q..I); focus `Focus::PatternRow`.
- Selection: `isStepSelected(i)`, only drawn when `stepFocus`.
- Playing step: `mini_acid_.currentStep()` (`pattern_edit_page_legacy.h:1087`).
- Playing pattern: `mini_acid_.current303PatternIndex(voice)` (checked only in CYBER, `:1305–1307`).

### Visual channels per cell

| State | BLACK (`drawMinimalStyle`) | CYBER (`drawRetroClassicStyle`) |
|---|---|---|
| Filled | Solid voice-colour block (`:1165`) | Dark cell, voice-colour note text |
| Accent | Orange LED (`:1165` area) | Orange note text + LED |
| Cursor | 2 px border, `COLOR_STEP_SELECTED` (`:1175`) | 1 px glow border, voice colour (`:1317`) |
| Selected | 3 px border, `COLOR_ACCENT` (`:1178`) | 1 px glow border, orange (`:1316`) |
| Playing | 1 px `COLOR_STEP_HILIGHT` outline + white scan line (`:1169`) | 2 px glow, voice colour + scan line (`:1316`) |

### Findings

**F1 — playhead shown while stopped (P1).**
`currentStep()` returns `0` when the transport is stopped (`miniacid_engine.cpp:833`).
BLACK tests only `playing == i` (`:1169`), so step 1 gets the playing outline while STOP.
CYBER avoids this only because it also requires `isPlayingPattern` (`:1316`), which needs
`mini_acid_.isPlaying()`.
Runtime: a STOP screenshot of BLACK SYNTH A shows an outline on the first cell. Needs an
explicit check on a stopped transport before the fix is confirmed.

**F2 — playhead on a non-playing pattern (P1).**
BLACK does not check which pattern is playing. If the user views pattern 3 while pattern 1 plays,
the playhead appears on pattern 3 at the playing step. CYBER checks this (`:1305–1307`).

**F3 — cursor and playhead are not separable in CYBER (P2).**
Both use `retroVoiceColor(voice_index_)`. The playhead is 2 px; the cursor is 1 px; the only other
difference is the white scan line. When the cursor sits on the playing step, the cell reads as a
single state. This matches V02/§4 of the concept.

**Note on focus.** Cursor and selection draw only when `stepFocus` (`:1087`, `:1316`). When
focus is on the pattern row, no step shows a cursor. This is intended by the current model
and should be preserved; the design should make the row focus visible instead.

### Visible but unverified

- Whether the BLACK filled-cell colour (voice colour, solid) hides the accent LED. Needs a hardware photo.
- Whether the 2 px cursor is distinguishable from the 3 px selection on bright fills.

## 3. MATERIAL BANK (PhrasePage, kPhraseCore)

### Owners

- Product focus: `product_focus_` (`phrase_page.cpp:538–541`).
- Bar cursor: `product_bar_cursor_`.
- Accepted generation: `GroovePuterState::GeneratedPhraseAcceptedSnapshot accepted`.
- Playing bar: `currentGeneratedBar(engine, accepted)` (`phrase_page.cpp:217–226`).

### Findings

**F5 — "current bar" is a song position, not a transport state (P2).**
`currentGeneratedBar` returns the bar when song mode is on, the accepted slot is playing in song
terms (`songPlaybackSlot() == accepted.songSlot`), and `currentSongPosition()` is in range. It does
not check `isPlaying()`. `drawGeneratedBarStrip` then fills the bar with the accent colour
(`:251–256`) whenever `bar == currentBar`.

So with song mode ON and transport STOPPED, the filled bar reads as "playing". It may be the
intended "position" marker, but the screen does not say which it is. This conflates position and
playback, which the concept's rule "PLAYING = реально воспроизводится" forbids.

Runtime check needed: enter MATERIAL BANK with song mode on, transport stopped, and an accepted
generation in range.

**F7 — OK.** Selection is an outline (`palette.text`), current is a fill (`palette.accent`),
inactive bars are `gridOff`. The two states use different channels, so they can be told apart.

## 4. SONG

### Owners

- Cursor row: `cursor_row` (see `song_page.cpp:2093`, `:2367`).
- Playhead: `mini_acid_.songPlayheadPosition()` (`song_page.cpp:1742`, `:2093`).
- Playing: `playing` and `songMode` and `paneSlot == playSlot`.
- Selection range: `min(cursorRow, playhead)` to `max(...)` (`:1792–1793`, `:2095–2096`) — the
  playhead is part of the selection range computation.

### Findings

**F6 — OK.** The playing overlay is gated correctly: `if (row == playhead && playing && songMode && paneSlot == playSlot)` (`song_page.cpp:1947`, `:2527`).

**Note — the playhead row is also the position row when stopped.** `if (row == playhead)` (`:1883`)
always runs; the playing fill is gated by `activePlay` (`:1884`). When stopped, the row keeps its
position label (`>n`, `:1947`) only while playing, so the stopped position is shown by the header
`B%02d/%02d` (`:1763`) and the cursor, not by a marker. This is acceptable; check it when the
SONG cursor sits next to the playhead.

**Note — playhead is in the selection model.** The comment at `:761` says the selection can include
the playhead column. Cursor and playhead are therefore coupled in the code. A visual fix that
separates them must not change selection semantics.

## 5. Footer prompt and help (F4)

The footer shows `[H] HELP` in the non-modified state (`ui_common.cpp:352`).

- Alt+H is the help binding: `miniacid_display.cpp:559`.
- Plain H falls through to the global block and is swallowed with no action: `:709–711`.
  Pages that use H themselves (e.g. GENRE harmony) consume it first.

So the footer label does not match the binding. A musician who presses H on SYNTH A gets nothing.

Runtime check: inconclusive. The emulator on this machine runs a Russian keyboard layout
(see `project_sdl_emulator_keyboard_layout_trap`), so keysym `h` does not reach the handler. The
code finding stands on its own; the device has its own key mapping and was not tested.

## 6. Recommended next steps (not implemented)

1. Fix F1 and F2 in the BLACK path: gate `playing` on `isPlaying()` and on the pattern check, as CYBER does.
   This is a logic fix in the drawing code, not a visual change.
2. Decide the footer label for help (F4): either `[Alt+H] HELP`, or make plain H open help
   on pages that do not use H. The second is a behaviour change and needs a decision.
3. F5: decide whether MATERIAL BANK's bar fill means "position" or "playing". Then label it, or gate it on `isPlaying()`.
4. Keep F6 and F7 as the reference for the playing channel in SONG and MATERIAL BANK.
5. Before any visual work on SYNTH A/B, confirm F1–F3 with runtime checks on a STOP state and a
   two-pattern state.

## 7. Not covered

- Hardware captures and interactive testing of any of the above.
- SAMPLER and MIDI PLAYER states.
- Whether the BLACK filled-cell colour is readable on the physical 240×135 panel.
