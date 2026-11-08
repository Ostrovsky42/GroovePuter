# GroovePuter UI experiments and MINIMAL theme pass (unversioned)

## Scope

**Base is not `v0.9.19`.** The working branch `feature/20260929-m0-musical-play-baseline`
is at `b5cf8e5b` (2026-10-01). `v0.9.19` (`a5f74f73`, 2026-10-11) is not an ancestor
of it: 240 commits on `v0.9.19` are absent here. The changes below are uncommitted on
top of `b5cf8e5b`. Version label: not assigned. The work stays on this branch and continues from here.
If it is later ported to the `v0.9.19` line, the five changes are small and can be
re-applied by hand.

Visual and label changes only, under the feature freeze. No music semantics, key
bindings, routing, persistence or generation changed.

## Changes

1. **CARBON renamed to MINIMAL.** The public theme cycle is `Alt+\` `MINIMAL <-> CYBER`.
   Internal enum names (`VisualStyle::MINIMAL`) are kept. Updated: theme name,
   `Alt+\` help line, `src/ui/docs/keys.md`, `MANUAL.md`, `agents_guide.md`, tests.
2. **Theme label consistency.** PROJECT page `Th:` and the settings `Theme` row
   show `MINIMAL` / `CYBER` (previously `MINI` / `RETRO`).
3. **`[H] HELP` contrast in MINIMAL.** Footer prompt uses the secondary token
   (`#82909C`, 5.96:1 on panel) instead of dim (`#46515B`, 2.40:1).
4. **Idle white piano keys in MINIMAL.** Idle keys use the secondary token instead of
   the bright text token. The held key keeps its active fill and focus border.
   CYBER is unchanged.
5. **MINIMAL accents less saturated.** Accent, accent2/active, focus, warning and
   danger chroma reduced to about 60% of the previous CARBON values:

   | Token | Before | After |
   |---|---|---|
   | accent | `4AC3E8` | `6AB2C8` |
   | accent2 / active | `69D89A` | `7FC29D` |
   | focus | `77DFFF` | `92D1E4` |
   | warning | `E6B85C` | `CAAF78` |
   | danger | `E35D72` | `C87884` |

   Background, panel and body text tokens are unchanged.

## Verification

- Host: `test_theme_selection_source_regressions.py`, `test_global_help_source_regressions.py`,
  `test_global_help_content`, `run_pattern_tie_ui_tests.sh` — PASS.
- SDL screenshot pass: 12 pages x 2 themes, before/after pixel diff. CYBER changes only
  in the header row (live time field); MINIMAL changes on all pages as intended.
- Screenshots are from the SDL emulator at 2x. Not yet checked on Cardputer ADV hardware.
  Not flashed.

## Not in 0.9.19.1

The "Instrument Console" concept (state grammar FOCUS / SELECTED / PLAYING / FILLED /
EMPTY / MUTED / DISABLED / ERROR, StateCell and FocusFrame components, page revisions)
is a proposal, not implemented. Before any of it lands:

- read-only audit of focus, selection and playing owners and of key handlers
  (D1 in the concept);
- three reference screens first: SYNTH A, MATERIAL BANK, SONG;
- SYNTH A/B filled cells in MINIMAL are still saturated blue. Not changed here.

## Source material

- `GroovePuter_themes_Black_Cyber.zip` (in `~/Downloads`): 24 screenshots and
  `README_UX.md`. Screenshots predate the 0.9.19.1 accent and key changes.
