# CLEAN theme — design spec (proposal, not applied)

Status: proposal for review. No palette values below are in the firmware yet.
Public name: `CLEAN` (internal enum stays `VisualStyle::MINIMAL`). Cycle: `Alt+\` `CLEAN <-> CYBER`.

## 1. Intent

CLEAN is the instrument-grade theme. CYBER is the expressive one. Both use the same page
structure; CLEAN differs by palette and by how much it lets through.

Reference character: small hardware instruments with one neutral surface, one functional
accent, and information carried by type, position and inversion rather than colour.

Rules:
1. Monochrome first. Text, rules and fills are neutral greys on black.
2. One accent hue, used only for live state: playing, recording-like activity, errors.
3. Contrast is selective. The eye lands on: focus, the playhead, and the value being changed.
   Everything else recedes.
4. Voices and tracks are identified by label and position, never by colour alone.
5. No glow, no gradients, no textures, no coloured panels.

Memory is not the goal of this theme. Clarity and professional finish are.

## 2. Palette (proposed)

| Token | HEX | Contrast on bg `#000000` | Use |
|---|---|---|---|
| background | `000000` | — | every surface |
| text | `F2F2EE` | 18.7:1 | values, notes, titles (warm white) |
| secondary | `9A9A94` | 7.4:1 | labels, units, help hint |
| dim | `5C5C58` | 3.1:1 | empty cells, disabled, inactive (never for a command the user can use) |
| rule | `3A3A38` | 1.8:1 | decorative rules and grid lines only, never a focus border |
| accent | `FF6A2B` | 7.4:1 | playing marker, active state, error text |
| focus fill | `F2F2EE` | — | inverted block behind the focused item |
| focus text | `000000` | 18.7:1 on focus fill | text inside the focused block |

No separate panel colour. Surfaces are separated by rules and spacing.

Warm white and warm grey are deliberate: they read as printed/anodised labels rather than
LCD-blue. Final values must be checked on the 240x135 panel before they are locked.

## 3. State grammar (no colour needed to read it)

| State | How it looks | Why this form |
|---|---|---|
| Focus | inverted block (white fill, black text) | strongest shape on screen; survives any backlight level |
| Selected (not focused) | 1 px text-coloured outline around the item | secondary to focus, still legible |
| Playing | accent 2 px bar on the top edge of the item, plus the accent playhead line | a marker, not a fill; does not compete with focus |
| Filled step / value | text-coloured glyph | no background needed |
| Empty step | dim dot centred | presence of a grid without filling it |
| Muted | dim glyph with a short tag (`M`) | visible without relying on colour |
| Error | accent text prefixed `!` | one exception to the monochrome rule |

Focus and playing never share a form: focus is an inverted block, playing is an accent bar.
This directly answers audit F3 (cursor vs playhead in one colour).

## 4. Identity without hue

Where CYBER uses voice colours, CLEAN uses:
- a letter or index that is always shown next to the voice name (`A`, `B`, `DR`);
- the voice's column/row position, which never changes;
- stroke weight for the active voice (2 px rule under its header).

This keeps the information for the drum grid (8 voices) and the song tracks without
a palette of eight colours.

## 5. Typography and layout

- Labels: secondary, upper case, short. Values: text colour, one size.
- Header row (12 px): page name left, transport and tempo right, always in the same
  positions across pages.
- Footer (14 px): one hint line in secondary; `ALT+H HELP` at the right in secondary.
- Content: a 4 px margin, grid of 8 px cells on pages that need one.
- Rules: 1 px, rule colour, only between major sections. At most three rules per page.

## 6. What changes per page (direction only)

- SYNTH A/B: note cells become text on a plain grid with rule-coloured outlines; focus is the inverted cell; playhead is the accent bar on the cell's top edge.
- DRUMS: 8 voice rows with letters; filled = glyph, empty = dim dot; the playhead is a 1 px accent column.
- SONG: track columns labelled A/B/DR; the playing row is marked by an accent bar on its left edge; the focused cell is inverted.
- MATERIAL / BANK: slots A–D as labelled boxes; the selected slot is outlined, the filled slot is inverted only when it is also focused.
- KEY / GENRE / FEEL: parameter rows; focus is the inverted row; values right-aligned.

## 7. Checks before locking

- Read every page at 240x135, physical device, with the backlight at the two lowest levels and the highest.
- Focus must be identifiable in under two seconds when transport is stopped (project criterion).
- Playing must be distinct from focus with the colour removed (greyscale photo check).
- The accent must be the only hue on screen during normal use.
- Any text below 4.5:1 must be a label the user does not act on.

## 8. Open questions for the owner

1. Accent hue: `FF6A2B` (warm orange, TE-like) or a cooler single hue?
2. Is the rule colour (`3A3A38`, 1.8:1) acceptable for section rules, given it is decorative?
3. Keep "CLEAN" as the public word? It is also the MIDI import profile value `CLEAN`
   (`project_page.cpp:849`); the two are unrelated but both appear on PROJECT.
