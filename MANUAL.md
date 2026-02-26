# MiniAcid Manual (Current)

This manual is the high-level guide for current firmware.
For exact hotkeys use `docs/keys_sheet.md`.
For page-deep behavior use focused docs in `docs/`.

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
MiniAcid separates responsibilities:
- `GENRE`: what is generated
- `GENERATOR`: generator constraints (regen-time)
- `FEEL`: timing perception (live)
- `TEXTURE/TAPE`: sound color (live)
- `GROOVE LAB`: mode/flavor/corridor control

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

## 3. Playback Basics
- `Space`: play/stop
- `[` / `]`: page navigation
- `Alt+1..0` / `Ctrl+1..0`: direct page jump
- `1..9,0`: global mute toggles

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

## 6. Tape Workflow
Tape page is performance-oriented:
- `X`: smart REC/PLAY/DUB flow
- `A`: CAPTURE
- `S`: THICKEN (safe one-cycle dub)
- `D`: WASH
- `G`: loop mute

Reference: `docs/TAPE_WORKFLOW.md`.

## 7. Safety
- Master high-cut is DSP-hardcoded (`kMasterHighCutHz` in `src/dsp/miniacid_engine.h`).
- Default is `16000 Hz`.
- Lowering this value in code gives stronger HF protection on compact speakers.

## 8. Docs Index
- `README.md` — project overview
- `docs/keys_sheet.md` — canonical key map
- `docs/GROOVE_LAB.md` — mode/flavor/corridors
- `docs/SONG_PAGE_QUICKSTART.md` — Song operations
- `docs/MIDI_IMPORT_GUIDE.md` — MIDI routing & smart import
- `docs/SONG_PAGE_STYLES.md` — Song style behavior
- `docs/GENRE_PAGE_STYLES.md` — Genre page behavior
- `docs/TAPE_WORKFLOW.md` — tape performance flow
- `docs/LONG_SONG_ARCHITECTURE.md` — paging & long song architecture
