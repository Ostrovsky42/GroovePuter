# GroovePuter Key Map — Cardputer ADV

This document mirrors the current runtime key routing. `Alt+H` is the on-device,
page-aware reference; README screenshots show the same primary footer hints.

## Input priority

1. An open help or workspace overlay owns input.
2. Hard-global shortcuts are handled before the page: `Fn+M`, workflow navigation,
   `Alt+H`, `Alt+P`, `Alt+W`, `Alt+X`, `Alt+M`, theme switching, and direct page jumps.
3. The active page gets first refusal, including `Space`, NOTE mode, Song, Phrase,
   and MIDI Player commands.
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
| `Space` | Active transport unless the page consumes it |
| `Alt+P` | MIDI Player |
| `Alt+V` | First GENERATE page |
| `Alt+W` | Waveform overlay, except Phrase explicit overwrite |
| `Alt+X` | LiveMix ON/OFF |
| `Alt+M` | Song mode ON/OFF |
| `Alt+\` | Toggle `CARBON <-> CYBER` |
| `1..9`, `0` | Track-mute fallback if the page does not consume the digit |
| `Esc`, `Backspace`, `` ` `` | Back, dismiss, or previous page |
| `Ctrl+Alt+Backspace` | Panic active notes and reset the project |

Workflows cycle in this order:

```text
PERFORM -> GENERATE -> HUB -> SONG -> SETTINGS
```

Pages inside each workflow:

```text
PERFORM:  MIDI KEYBOARD -> MIDI PLAYER
GENERATE: GENRE -> FEEL -> GENERATION
HUB:      OVERVIEW -> SYNTH A -> SYNTH B -> DRUMS -> SYNTH A SOUND -> SYNTH B SOUND
SONG:     SONG -> PHRASE CORE
SETTINGS: PROJECT / SETUP
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

Direct note input remains active while transport runs. Step-based tools follow the
project transport timeline and use the existing event router and MIDI dispatcher.

## GENRE 1/3

| Key | Action |
|---|---|
| `Tab` / `Up/Down` | Select genre, variant, rhythm, morph, or apply-policy field |
| `Left/Right` | Adjust the selected field |
| `Alt+Left/Right` | Morph the selected variant |
| `Enter` | Apply profile or materialize according to the selected policy |
| `M` | Cycle `PROFILE ONLY`, `MATERIALIZE`, `MATERIALIZE+BPM` |

`RHYTHM` cycles `AUTO` plus only the stable rhythm identities compatible with the
pending Genre/Variant. A manual identity remains fixed while pattern address and
P-level vary its realization. Changing Genre/Variant resets an incompatible manual
identity to `AUTO` and shows a short notice.

GENRE owns musical corridor and vocabulary. It does not own FEEL or sound design.

## FEEL 2/3

| Key | Action |
|---|---|
| `Tab` / `Up/Down` | Select FEEL field |
| `Left/Right` | Adjust swing, timing humanize, velocity humanize, or preset |
| `Shift` / `Ctrl` | Accelerated adjustment |
| `Enter` / `Space` | Apply the selected FEEL preset |

FEEL changes timing and velocity only. Browsing a preset does not mutate Scene until
apply. Digits remain available to the global mute fallback; they are not FEEL hotkeys.

## GENERATION 3/3

| Key | Action |
|---|---|
| `Left/Right` | Move target Song row by one |
| `Up/Down` | Move target Song row by eight |
| hold `Arrows` | Accelerate target browsing |
| `Enter` / `G` | Generate material into a free slot and materialize the selected row |

Target browsing is UI-only. Song position changes only when materialization succeeds.
Phrase length is owned by Phrase Core, not by this page. Sound design remains owned by
the synth, Tape, delay, distortion, and related FX controls.

## PATTERN MATRIX

Pattern addresses use `PAGE + BANK + SLOT`: `1A1` through `16B8`. PAGE, BANK,
and SLOT are independent coordinates. Changing page preserves bank and slot;
changing bank preserves page and slot; changing slot preserves page and bank.

```text
1A1 --page 2--> 2A1
2A1 --slot 2--> 2A2
2A2 --bank B--> 2B2
2B2 --page 3--> 3B2
```

On Synth A/B note-editor screens, the note/pattern header prints the composite address
directly (for example `2A2 TB303`), and the global status chrome must show the same
page/bank/slot identity.

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
| `Q..I` | Assign an existing pattern without regeneration |
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
| `Ctrl+1..8` | Select pattern page 1..8 |
| `Ctrl+Fn+1..8` | Select pattern page 9..16 |
| `P` | Move cursor to playhead |
| `Ctrl+W/S` | Jump 8 rows |
| `Ctrl+Alt+W/S` | Jump 32 rows |
| `Alt+Q/E/R/T` | Save markers 1..4 |
| `Ctrl+Alt+Q/E/R/T` | Jump to markers 1..4 |
| `Alt+,/.` | Jump to Song top/end |
| `Backspace` / `Tab` | Clear cell / selection |
| `Alt+Backspace` | Clear full Song |

`Q..I` changes only SLOT; PAGE and the target-track BANK remain unchanged.
`Alt+[` / `Alt+]` moves one pattern page at a time. `NO EMPTY PATTERN SLOTS`
means generation changed neither Song references nor pattern content.

## PHRASE CORE

| Key | Action |
|---|---|
| `1..4` | Select Phrase A/B/C/D |
| `Up/Down` | Capture length `1/2/4/8` bars |
| `Left/Right` | Preview saved Phrase bar |
| `R` | Cycle capture role |
| `Shift+R` | Previous role |
| `P` | Cycle derive parent |
| `Enter` | Capture current Song region |
| `D` | Derive parent into selected slot |
| `W` | Write to an empty Song destination |
| `Alt+W` | Explicit destructive overwrite |
| `Backspace` / `Delete` | Clear selected Phrase |

Phrase storage is `REFERENCE VIEW / REF MUTABLE`. Editing a referenced pattern changes
the Phrase material; save/load preserves valid slots and cleared slots.

## OVERVIEW / SEQUENCER HUB

| Key | Action |
|---|---|
| `Up/Down` | Select track |
| `Left/Right` | Select step |
| `Fn+Left/Right` | Selected-track volume -/+ |
| `-` / `=` | Track volume compatibility alias |
| `X` | Toggle hit/note |
| `A` | Toggle accent |
| `Enter` | Open track detail |
| `Esc` / `Backspace` | Return to overview |
| `Space` | Transport |
| `Q..I` | Select local pattern |
| `B` | Toggle pattern bank |
| `Ctrl+C/V` | Copy / Paste |

Internal HUB track volumes are scene/project state: saving a project at `0%` keeps that
synth or drum lane at `0%` after reboot/load. In HUB MIDI mode, `H` returns to Player,
`1..9` mutes physical SMF tracks, plain `Left/Right` edits the selected route override
(`AUTO`, `CH1..CH10`), and `Fn+Left/Right` changes the selected physical-track level in
5% steps. MIDI levels are session-only and reset to `100%` for a newly loaded SMF.
Confirmed routes remain persisted per file when the file identity still matches.

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

`Alt+H` always opens help; unmodified `H` remains the Player <-> HUB MIDI shortcut.
