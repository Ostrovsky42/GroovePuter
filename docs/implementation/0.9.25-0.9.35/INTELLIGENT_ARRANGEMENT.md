# Intelligent Arrangement audit — 0.9.31

## Existing owner

Song → FORM is the arrangement owner. `EnergyCurve` derives an insertion from
an existing block of up to eight Song rows. It copies those rows into ordinary,
editable Song positions while choosing which existing tracks play at each
section's energy level. A section is currently four or eight bars with energy
0–6. Genre-specific curves exist for Acid, Dub, Trip-Hop, House, Techno and UK
Garage; other genres use WAVE. BUILD, DROP and WAVE are also direct presets.

The source block comes from the grid selection or the contiguous material under
the cursor. FORM inserts after that block. It does not allocate new material
slots, silently regenerate source Patterns or create another playback model.
Once applied, each inserted row is regular Song data and later grid edits remain
authoritative. `SongPage::applyForm()` passes the change through
`commitSongMutation()`, so capacity failure is evaluated on a staged Song and a
successful form is one Song undo action. Section boundaries are bar-aligned.

The starting curves expose the requested functions without persisting role
types. Keys 1–3 keep BUILD, DROP and WAVE; keys 4–8 select INTRO, MAIN,
DEVELOPMENT, BREAK and OUTRO. The selected name stays visible beside the source
range after its toast expires. Editing any section changes the label to CUSTOM.
Genre curves continue to load on entry when the active genre changes.

| Role | Default contour (energy code per section) | Bars |
|---|---|---:|
| INTRO | 1 → 2 → 3 | 12 |
| MAIN | 4 → 4 | 16 |
| DEVELOPMENT | 2 → 6 → 5 → 4 | 16 |
| BREAK | 4 → 5 → 0 → 2 | 16 |
| BUILD | 1 → 2 → 3 → 4 → 4 | 20 |
| DROP | 2 → 3 → 4 → 1 → 4 | 20 |
| OUTRO | 4 → 3 → 2 → 1 | 16 |

These curves set track participation from the existing source block. They do
not add or vary motif notes. Every curve remains editable before insertion; after
insertion the Song rows remain independently editable.

## 0.9.31 change

`EnergyCurve::apply()` previously bounded the bar-count helper to eight
sections but later iterated the unvalidated `count`. A malformed count above
the fixed section array could therefore read out of bounds. `validCurve()` now
rejects counts above eight, section lengths other than four/eight bars, and
energy values above six before any source read or Song mutation. Rejection is
`BadRange` and leaves the staged Song unchanged.

The host test covers each malformed input and compares the complete Song,
including track cells and step overrides, before and after rejection.

## Contract boundary

The requested INTRO, MAIN, DEVELOPMENT, BREAK, BUILD, DROP and OUTRO meanings
are arrangement roles, not new persisted Song row types. Their transient
presets are starting contours in the existing FORM curve. No semantic role
label survives materialization, and the system does not create independent
motif variations. Musical suitability of a particular curve is not established
without listening.

No second arrangement engine or role field is introduced in 0.9.31. This keeps
Scene/Song rows as the source of truth and preserves old project compatibility.

## Verification status

- `tests/test_energy_curve.cpp`: passed, including role contours and
  malformed-curve atomicity.
- AddressSanitizer and UndefinedBehaviorSanitizer builds of the focused test:
  passed.
- `tests/test_song_form_genre_source_regressions.py`: passed, including
  keyboard range and transient preset labeling.
- Canonical host suite: passed before the role-preset UI extension; the focused
  domain test and UI source regression passed after that extension.
- SDL full app build and interactive FORM smoke: passed after the label fix.
  `xdpyinfo` confirmed display `:1`; the 480×270 SDL window represents the
  240×135 logical screen. Screenshots are in `/tmp/gp-0931-form-smoke/`.
  Tested Alt+7 → F, role keys 1–8, the INTRO label after toast expiry, UP
  changing it to CUSTOM, and Escape back to GRID. SDL used dummy audio and a
  temporary US XKB map, restored to the session's `us,ru` layout afterward.
- Default Cardputer ADV and FS1B firmware builds: passed after the label fix;
  see `RESOURCE_BUDGET.md` for sizes, memory and fingerprints.
- Hardware, keyboard comfort and musical listening: `NOT TESTED`.
