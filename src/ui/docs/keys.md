# GroovePuter Key Map — Cardputer ADV

This document mirrors the current runtime key routing. `Alt+H` is the on-device,
page-aware reference; `README.md` screenshots show the same primary footer hints.

## Input priority

1. An open help or workspace overlay owns input.
2. Hard-global shortcuts are handled before the page: `Fn+M`, workflow navigation,
   `Alt+H`, `Alt+P`, `Alt+W`, `Alt+X`, `Alt+M`, theme switching and direct page jumps.
3. The active page gets first refusal, including `Space`, NOTE mode, Song commands and
   MIDI Player controls.
4. Supported pages may route unmodified note keys to `PerformanceKeyboard`.
5. Remaining keys fall through to page navigation and global track mutes.

## Global shortcuts

| Key | Action |
|---|---|
| `Alt+H` | Toggle page-aware help |
| `Up/Down` in help | Scroll one line |
| `Left/Right` in help | Jump to top/end |
| `Fn+M` | Workspace launcher |
| `Fn+Tab` / `Fn+Shift+Tab` | Next / previous workflow |
| `[` / `]` | Previous / next page inside the workflow |
| `Fn+[` / `Fn+]` | Previous / next workflow |
| `Alt+[` / `Alt+]` | Previous / next detailed page |
| `Alt/Fn+1..0` | Direct page jump |
| `Space` | Active transport, unless the page consumes it |
| `Alt+P` | MIDI Player |
| `Alt+V` | MODE / FLAVOR (Groove Lab) |
| `Alt+W` | Waveform overlay |
| `Alt+X` | LiveMix ON/OFF |
| `Alt+M` | Song mode ON/OFF |
| `Alt+\` | Toggle `CARBON ↔ CYBER` |
| `1..9`, `0` | Track-mute fallback if the page does not consume the digit |
| `Esc`, `Backspace`, `` ` `` | Back, dismiss, or previous page |
| `Ctrl+Alt+Backspace` | Panic active notes and reset the project |

Workflows cycle in this order:

```text
PERFORM → GENERATE → HUB → SONG → SETTINGS
```

## MIDI KEYBOARD / PERFORM

| Key | Action |
|---|---|
| `QWERTYUIOP` | Upper scale-aware manual |
| `ASDFGHJKL` | Lower manual, one octave below |
| `N` | NOTE mode ON/OFF |
| `\` | Cycle internal/USB/DX/drum target |
| `,` / `.` | Previous / next scale |
| `-` / `=` | Octave down / up |
| `X` | Panic the live-owned target |
| `Tab` | Open PERFORMANCE TOOLS |
| `1..8` in tools | ARPEGGIATOR, DIRECTION, CHORD, MEMORY, STRUM, RATCHET, EUCLIDEAN, ROTATE |
| `Shift+1..8` | Cycle the adjustable tool backward |
| `Esc` / `` ` `` | Close the tools layer |

When Pattern/Song transport owns Synth A, note keys remain consumed and do not emit
competing `NoteOn` events.

## GENRE

| Key | Action |
|---|---|
| `Tab` | Cycle Genre / Texture / Recipe lane |
| `Arrows` | Move or adjust the active lane |
| `Enter` | Apply the selected recipe |
| `Space` / `M` | Cycle apply mode |
| `Fn+Up/Down` | Morph recipe |
| `C` | Curated / Advanced catalog |
| `G` | Acid / Minimal groove mode |
| `Ctrl+1..2` | Synth A bank A/B |
| `Q..I` | Synth A pattern 1..8 |

## SYNTH A / SYNTH B PATTERN

| Key | Action |
|---|---|
| `Tab` | Pattern / automation subpage |
| `Q..I` | Select pattern 1..8 |
| `B` | Toggle bank A/B |
| `Arrows` | Move cursor |
| `Shift/Ctrl+Arrows` | Extend selection |
| `A/Z` | Note +/- |
| `S/X` | Octave +/- |
| `Alt+Left/Right` | Rotate pattern |
| `Alt/Ctrl+A` | Accent |
| `Alt/Ctrl+S` | Slide |
| `F` | Cycle step FX |
| `R`, `Backspace`, `Delete` | Clear step / selection |
| `Alt+Backspace` | Clear whole pattern |
| `G` | Randomize pattern |
| `Ctrl+C/V` | Copy / Paste |
| `Esc` / `` ` `` | Clear selection |

## SYNTH A / SYNTH B SOUND

| Key | Action |
|---|---|
| `Tab` | Main / More parameters |
| `Left/Right` | Focus control or change value |
| `Up/Down` | Adjust value or select row |
| `Shift` / `Ctrl` | Fine adjustment |
| `Ctrl+1..2` | Pattern bank A/B |
| `Q..I` | Pattern selection when NOTE mode is off |
| `A/Z S/X D/C F/V` | Quick parameter controls |
| `T/G` | Oscillator +/- |
| `Y/H` | Filter type +/- |
| `N/M` | Distortion / Delay |
| `Ctrl+Z/X/C/V` | Reset quick parameter |

## DRUMS

| Key | Action |
|---|---|
| `Tab` | Sequencer / automation subpage |
| `Q..I` | Select pattern 1..8 |
| `B` | Toggle bank A/B |
| `Arrows` | Move cursor |
| `Shift/Ctrl+Arrows` | Extend selection |
| `Enter` | Toggle hit |
| `A` | Toggle accent |
| `G` | Randomize pattern |
| `Ctrl+G` | Randomize focused voice |
| `Alt+G` | Chaos-randomize the full drum pattern |
| `Backspace` / `Delete` | Clear hit / selection |
| `Alt+Backspace` | Clear whole pattern |
| `Ctrl+C/V` | Copy / Paste |

## SONG

| Key | Action |
|---|---|
| `Arrows` | Move cursor |
| `Shift/Ctrl+Arrows` | Extend selection |
| `Enter` | Jump to the referenced pattern editor |
| `Q..I` | Assign an existing pattern without regenerating it |
| `G` | Generate material into a free slot and assign the selected cell |
| double `G` | Generate and materialize the current row atomically |
| `Alt+G` | Generate the selected area |
| `Ctrl+G` | Cycle Song generator mode |
| `B` | Flip referenced pattern bank |
| `Ctrl+N` / `Ctrl+M` | Insert / delete row |
| `Alt+B` | Edit Song slot A/B |
| `Ctrl+B` | Play Song slot A/B |
| `V` | Toggle DR/VO lane |
| `X` | Split compare |
| `L` | Loop-lock around the playhead |
| `Ctrl+L` | Toggle loop mode |
| `Ctrl+R` | Reverse playback |
| `Alt+X` | LiveMix ON/OFF |
| `Ctrl+C/V` | Copy / Paste |
| `Ctrl+1..8` | Jump to edit page 1..8 |
| `P` | Move cursor to playhead |
| `Ctrl+W/S` | Jump 8 rows |
| `Ctrl+Alt+W/S` | Jump 32 rows |
| `Alt+Q/E/R/T` | Save markers 1..4 |
| `Ctrl+Alt+Q/E/R/T` | Jump to markers 1..4 |
| `Alt+,/.` | Jump to Song top/end |
| `Backspace` / `Tab` | Clear cell / selection |
| `Alt+Backspace` | Clear full Song |

`NO EMPTY PATTERN SLOTS` means generation changed neither Song references nor pattern
content.

## OVERVIEW / SEQUENCER HUB

| Key | Action |
|---|---|
| `Up/Down` | Select track |
| `Left/Right` | Select step |
| `-` / `=` | Track volume |
| `X` | Toggle hit/note |
| `A` | Toggle accent |
| `Enter` | Open track detail |
| `Esc` / `Backspace` | Return to overview |
| `Space` | Transport |
| `Q..I` | Select local pattern |
| `B` | Toggle pattern bank |
| `Ctrl+C/V` | Copy / Paste |

In HUB MIDI mode, `H` returns to Player, `1..9` mutes physical SMF tracks and `C`
edits the selected track route override (`AUTO`, `CH1..CH10`). Confirmed routes are
persisted per file and restored across reloads and reboots when the file identity still
matches; changed, stale, corrupt, or unknown files start at `AUTO`.

## FEEL / TEXTURE

| Key | Action |
|---|---|
| `Tab` | Feel / Presets focus |
| `Up/Down` | Select row |
| `Left/Right` | Change value |
| `Enter` / `Space` | Apply or cycle the selected value |
| `Ctrl+1..2` | Synth A bank A/B |
| `Q..I` | Synth A pattern 1..8 |

Digits remain available to the global mute fallback; they are not Feel preset hotkeys.

## MODE / FLAVOR

| Key | Action |
|---|---|
| `Tab` / `Up/Down` | Focus row |
| `Left/Right` | Change value |
| `Enter` | Run selected action |
| `Space` | Preview / regenerate |
| `A/B` | Apply to Synth A/B |
| `D` | Apply to Drums |
| `G` | Generate phrase |
| `M` | Toggle macros |

## ADV GENERATOR

| Key | Action |
|---|---|
| `Tab` | Next parameter group |
| `Up/Down` | Select row |
| `Left/Right` | Adjust value |
| `Shift/Ctrl/Alt` | Fast adjustment |
| `Enter` / `Space` | Apply selected preset |
| `T` | SD benchmark |

## PROJECT / SETUP

| Key | Action |
|---|---|
| `Tab` | Next section; MIDI browser opens the import matrix |
| `Up/Down` | Select row/file |
| `Left/Right` | Adjust value or dialog focus |
| `Enter` | Open or activate |
| `G` | Jump to GENRE |
| `Esc` / `Backspace` | Close dialog or go up a MIDI directory |
| `X` | Delete selected scene/file in supported dialogs |

## MIDI PLAYER

| Key | Action |
|---|---|
| `Enter` | Open selected MIDI file |
| `Space` | MIDI transport |
| `H` | Open HUB MIDI / return to Player |
| `1..9` | Physical SMF track mute |
| `U` | Physical-track mute mixer |
| `I` | Channel inspector |
| `S` | Structural inspector |
| `D` | Performance/throughput panel |
| `B` / `Backspace` | Files or previous panel |
| `Arrows` | Select, seek, scroll, or adjust BPM according to the active panel |
| `C` | Clock source |
| `T` | Tempo mode |
| `M` | RAW / SEQTRAK routing |
| `G` | Groove transport/follow |
| `R` | Restart file |
| `V` | Velocity boost |
| `X` | Panic SMF-owned notes |

`Alt+H` always opens help; unmodified `H` remains the Player ↔ HUB MIDI shortcut.
