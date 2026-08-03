# GroovePuter Manual (Current)

This manual describes user-visible behavior available in the current firmware.
For the canonical product direction, execution order, acceptance metrics, and deferred scope, read [`PLAN.md`](PLAN.md).
For exact hotkeys use `docs/keys_sheet.md` and `src/ui/docs/keys.md`.
For page-deep behavior use focused docs in `docs/`.

This manual must follow shipped behavior. It does not establish a second roadmap.

## Quick Keys (Most Used)
- `Space`: play/stop
- `Arrows`: move cursor / navigate lists
- `Enter`: confirm/apply/toggle focused item
- `Tab`: switch focus/section on many pages
- `[` / `]`: previous/next page
- `Ctrl+[` / `Ctrl+]`: switch editing page context (Song Page)
- `Q..I`: choose pattern slot `1..8` in Pattern/Drum/Song contexts
- `B`: quick A/B bank toggle (Pattern/Drum) or bank flip in Song cell/selection
- `Alt+B`: edit song slot `A/B`
- `Ctrl+B`: play song slot `A/B`
- `X`: split compare (Song) or primary action on Tape page
- `Esc`: back (or clear selection in editors)

## 1. Core Concept

GroovePuter separates four musical responsibilities:

- `GENRE`: musical language, constraints, and recommended corridors;
- `GENERATOR`: how note and drum events are created or changed;
- `FEEL`: how existing material moves in time;
- `TEXTURE`: how the current material sounds.

The invariant is:

```text
GENRE != FEEL != GENERATOR != TEXTURE
```

Changing one axis should not silently overwrite the others.

The intended long-term musical hierarchy is:

```text
step -> bar -> phrase -> section -> song
```

Current firmware already provides step, pattern, multi-bar generation, Song, FEEL, TEXTURE, and MIDI workflows. The missing Phrase layer and the order in which it will be introduced are defined in `PLAN.md`; this manual does not describe those future controls as shipped.

## 2. Main Pages
- `Genre`: style + apply policy (`SND`, `S+P`, `S+T`)
- `Pattern Edit (A/B)`: note/accent/slide editing with selection v2
- `Synth Params (A/B)`: swappable synth voicing (TB-303, OPL2, AY/PSG, SID)
- `Drum Sequencer`: drum grid editing with selection v2
- `Song`: arrangement with dual slots `A/B`, split compare, live mix
- `Sequencer Hub`: compact overview/edit surface
- `Feel/Texture`: timing, coloration, and Drum FX macros
- `Generator`: generation parameters
- `Project`: scenes, groove section, LED section
- `Mode (GROOVE LAB)`: groove mode/flavor and budget-aware preview
- `Tape`: looper/FX performance workflow
- `SMF Player`: realtime MIDI-file playback and routing

## 3. Playback Basics
- `Space`: play/stop
- `[` / `]`: page navigation
- `Alt+1..0` / `Ctrl+1..0`: direct page jump
- `1..9,0`: global mute toggles

The current input system uses page-first-refusal. A page may own alphabetic keys before NOTE mode receives them. When transport owns Synth A, live note keys may be reserved without producing `NoteOn`. Wave 1 in `PLAN.md` adds visible `NOTE / CMD / LOCAL / LOCK` status without changing that behavior in the same PR.

## 4. Song Workflow (recommended)
1. Build core patterns in Pattern/Drum pages
2. Arrange in Song page (`Q..I` assign, `Bksp/Tab` clear)
3. Use slots:
- `Alt+B`: edit slot A/B
- `Ctrl+B`: play slot A/B
- `X`: split compare
- `Alt+X`: live mix on/off
4. Use selection v2 for block copy/paste

Detailed Song controls: `docs/SONG_PAGE_QUICKSTART.md`.

## 5. Groove Workflow
Use `GROOVE LAB` when you want controlled variation:
- choose `Mode`
- choose `Flavor`
- inspect corridor/budget line
- preview regenerate

Reference: `docs/GROOVE_LAB.md`.

## 6. MIDI Workflow

The `dev` line includes sample-timed USB-MIDI output, MIDI Clock/transport paths, Pattern/live dispatch, and realtime SMF playback. All USB writes must continue through the accepted single dispatcher.

Hardware-dependent routing and lifecycle behavior remains beta until the relevant stage acceptance is complete. Do not infer that the external target recorded data merely because GroovePuter completed local transmission.

## 7. Tape Workflow
Tape page is performance-oriented:
- `X`: smart REC/PLAY/DUB flow
- `A`: CAPTURE
- `S`: THICKEN (safe one-cycle dub)
- `D`: WASH
- `G`: loop mute

Reference: `docs/TAPE_WORKFLOW.md`.

## 8. Safety
- Master high-cut is DSP-hardcoded (`kMasterHighCutHz` in `src/dsp/miniacid_engine.h`).
- Default is `16000 Hz`.
- Lowering this value in code gives stronger HF protection on compact speakers.
- Monitor `[PERF]` telemetry; `underruns` must not continually increase during normal playback and navigation.

## 9. Docs Index
- `PLAN.md` — canonical product direction, priority order, metrics, and deferred scope
- `README.md` — project overview and current branch capabilities
- `MANUAL.md` — current user-visible behavior
- `docs/keys_sheet.md` — canonical key map
- `src/ui/docs/keys.md` — page-by-page key behavior
- `docs/GROOVE_LAB.md` — mode/flavor/corridors
- `docs/SONG_PAGE_QUICKSTART.md` — Song operations
- `docs/MIDI_IMPORT_GUIDE.md` — MIDI routing & smart import
- `docs/SONG_PAGE_STYLES.md` — Song style behavior
- `docs/GENRE_PAGE_STYLES.md` — Genre page behavior
- `docs/TAPE_WORKFLOW.md` — tape performance flow
- `docs/LONG_SONG_ARCHITECTURE.md` — paging & long song architecture
- `docs/stages/` — subordinate implementation specifications and acceptance records
