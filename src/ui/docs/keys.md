# GroovePuter 0.9.1 Key Map — Cardputer ADV

This is the canonical external key reference for the current runtime. `Alt+H` shows
the matching page-aware help on the device.

## Input priority

1. Open help/workspace overlays own input.
2. Hard-global shortcuts are handled before pages.
3. The active page gets first refusal.
4. Supported pages may route unmodified note keys to performance input.
5. Remaining keys fall through to navigation/global mutes.

This ordering is why Phrase `Alt+W`, NOTE ENTRY, Song assignment, MIDI mutes and other
page-local commands do not collide with global fallbacks.

## Workflows

```text
PERFORM:  MIDI KEYBOARD -> MIDI PLAYER
GENERATE: GENRE -> FEEL
HUB:      OVERVIEW -> SYNTH A -> SYNTH B -> DRUMS
SONG:     SONG -> PHRASE CORE
SETTINGS: PROJECT / SETUP
```

There are 11 active pages. Legacy persisted `GENERATION`/`TEXTURE` IDs resolve to FEEL.
Legacy standalone Synth SOUND IDs resolve to SYNTH A/B; sound editing lives in local
`NOTES -> KNOBS -> MORE` tabs.

## Global shortcuts

| Key | Action |
|---|---|
| `Alt+H` | Toggle page-aware help |
| `Up/Down` in help | Scroll one line |
| `Left/Right` in help | Jump to top/end |
| `Fn+M` | Workspace launcher |
| `Fn+Tab` / `Fn+Shift+Tab` | Next / previous workflow |
| `[` / `]` | Previous / next page inside workflow |
| `Fn+[` / `Fn+]` | Previous / next workflow |
| `Alt+[` / `Alt+]` | Previous / next pattern page |
| `Alt/Fn+1..0` | Direct page jump |
| `Space` | Active transport unless page consumes it |
| `Alt+P` | MIDI Player |
| `Alt+V` | GENRE (first GENERATE page) |
| `Alt+W` | Waveform overlay except Phrase REPLACE |
| `Alt+X` | LiveMix ON/OFF |
| `Alt+M` | Song mode ON/OFF |
| `Alt+\` | `CARBON <-> CYBER` |
| `1..9`, `0` | Track-mute fallback if page does not consume digit |
| `Esc`, `Backspace`, `` ` `` | Back/dismiss/previous page |
| `Ctrl+Alt+Backspace` | Panic active notes and reset project |

## MIDI KEYBOARD / PERFORM

| Key | Action |
|---|---|
| `QWERTYUIOP` | Upper scale-aware manual |
| `ASDFGHJKL` | Lower manual, one octave below |
| `N` | NOTE mode ON/OFF |
| `\` | Cycle internal/USB/DX/drum target |
| `,` / `.` | Previous / next scale |
| `-` / `=` | Octave down / up |
| `X` | Panic live-owned target |
| `Tab` | Open/close PERFORMANCE TOOLS |

### PERFORMANCE TOOLS

| Key | Action |
|---|---|
| `1` | ARPEGGIATOR ON/OFF |
| `2` | Arpeggiator DIRECTION |
| `3` | CHORD mode |
| `4` | Capture/clear chord MEMORY |
| `5` | STRUM amount |
| `6` | RATCHET count |
| `7` | EUCLIDEAN pulses |
| `8` | EUCLIDEAN ROTATE |
| `9` | Receiver `MONO/POLY` |
| `-` / `_` | Velocity -10 |
| `=` / `+` | Velocity +10 |
| `Shift+1..8` | Cycle adjustable tool backward when modifier is available |
| `Esc` / `` ` `` | Close tools layer |

Performance velocity is clamped to `10..120`. Receiver MONO/POLY is external-MIDI
ownership; internal Synth A/B remain sequencer/pattern instruments.

## GENRE 1/2

| Key | Action |
|---|---|
| `Tab` / `Up/Down` | Select Genre/Variant/Rhythm/Apply field |
| `Left/Right` | Adjust selected field |
| `Enter` | Apply according to selected policy |
| `G` | Explicit full Stage 15 generation |
| `P` | `P1 CANON -> P2 VAR -> P3 TRANS` |
| `M` | Cycle `PROFILE`, `MATERIALIZE`, `MATERIALIZE+BPM` |

Repeated accepted `G` requests reroll the same selected musical tuple through the
bounded session attempt stream. While PLAY is active, full material publishes at the
next real `BAR_START`; stopped generation is immediate.

`RHYTHM` is `AUTO` or a compatible stable identity. Browsing does not silently mutate
FEEL or synth sound.

## FEEL 2/2

| Key | Action |
|---|---|
| `Tab` / `Up/Down` | Select FEEL field |
| `Left/Right` | Adjust selected value |
| hold `Left/Right` | Accelerated adjustment |
| `Shift` / `Ctrl` / `Alt` + arrows | Fast adjustment where supported |
| `Enter` / `Space` on PRESET | Apply selected FEEL preset |
| `P` | Cycle shared P1/P2/P3 request level |

FEEL fields are PROFILE, SWING, FEEL AMOUNT, VELOCITY VAR, REPEATS `1/2/4/8`, PRESET.
FEEL owns timing/velocity only; it does not choose pitch, role or timbre.

## Pattern address model

```text
PAGE 1..16 x BANK A/B x SLOT 1..8 = 256 addresses
```

Example: `2B7`. PAGE, BANK and SLOT are independent coordinates.

## SYNTH A / SYNTH B

`Tab` cycles one parent-owned tab state:

```text
[N]KM  NOTES
N[K]M  KNOBS
NK[M]  MORE
```

There are no separate runtime SOUND pages.

### NOTES

| Key | Action |
|---|---|
| `Tab` | NOTES -> KNOBS -> MORE |
| `Q..I` | Select pattern slot 1..8 outside NOTE ENTRY |
| `B` | Toggle bank A/B |
| `Ctrl+1..2` | Select bank A/B directly |
| `Alt+[` / `Alt+]` | Previous / next pattern page |
| `Arrows` | Move step cursor |
| `Shift/Ctrl+Arrows` | Extend selection |
| `N` | NOTE ENTRY ON/OFF |
| `G` | Reroll selected synth lane when NOTE ENTRY is OFF |
| `Ctrl+C/V` | Copy / Paste |
| `Backspace` / `Delete` | Clear step/selection |
| `Alt+Backspace` | Clear whole pattern |
| `Esc` / `` ` `` | Clear selection |

Plain `G` outside NOTE ENTRY uses the active Genre/Variant/Rhythm/P-level/harmony
identity and changes only the selected Synth A or B lane. During PLAY the new lane
publishes at `BAR_START`. Inside NOTE ENTRY, `G` is a note key and generation does not
steal it.

NOTE ENTRY uses the two QWERTY rows for pitches; arrows move the grid, Backspace clears
the current step, Enter advances, and `;` recalls the last entered pitch.

### KNOBS / MORE

| Key | Action |
|---|---|
| `Tab` | Cycle NOTES / KNOBS / MORE |
| `Left/Right` | Focus/change value |
| `Up/Down` | Adjust value/select row |
| modifier + arrows | Fine/accelerated adjustment where supported |
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
| `Ctrl+1..2` | Direct bank A/B |
| `Alt+[` / `Alt+]` | Previous / next pattern page |
| `Arrows` | Move grid cursor |
| `Shift/Ctrl+Arrows` | Extend selection |
| `Enter` | Toggle hit |
| `A` | Toggle accent |
| `G` | Drums-only strong generation at current P-level |
| `Ctrl+G` | Randomize focused drum voice |
| `Alt+G` | Full-pattern CHAOS |
| `Ctrl+Alt+G` | Stage 12 phrase audition/probe |
| `P` | Cycle shared P1/P2/P3 request level |
| `Ctrl+C/V` | Copy / Paste |
| `Backspace` / `Delete` | Clear hit/selection |
| `Alt+Backspace` | Clear whole pattern |

## SONG

| Key | Action |
|---|---|
| `Left/Right` | Move `Synth A -> Synth B -> Drums`; outer edge crosses edit Song A/B |
| `Up/Down` | Move Song row |
| `Shift/Ctrl+Arrows` | Extend selection; never crosses Song slot |
| `Enter` | Jump to referenced pattern editor |
| `Q..I` | Assign existing slot from visible `PAT:A/B` context |
| `G` | Generate safe free material and assign selected cell |
| double `G` | Materialize Synth A + Synth B + Drums for current row |
| `Alt+G` | Generate selected area |
| `Ctrl+G` | Cycle Song generator mode |
| `B` | Toggle visible `PAT:A/B` assignment context |
| `Alt+B` | Flip stored-reference/selection bank |
| `Ctrl+B` | Play Song slot A/B |
| `Ctrl+N` / `Ctrl+M` | Insert / delete row |
| `V` | Toggle DR/VO lane |
| `X` | Split compare |
| `L` | Loop-lock around playhead |
| `Ctrl+L` | Loop mode |
| `Ctrl+R` | Reverse playback |
| `Alt+X` | LiveMix ON/OFF |
| `Ctrl+C/V` | Copy / Paste |
| `Ctrl+1..8` | Pattern page 1..8 |
| `Ctrl+Fn+1..8` | Pattern page 9..16 |
| `P` | Cursor to playhead |
| `Ctrl+W/S` | Jump 8 rows |
| `Ctrl+Alt+W/S` | Jump 32 rows |
| `Alt+Q/E/R/T` | Save markers 1..4 |
| `Ctrl+Alt+Q/E/R/T` | Jump markers 1..4 |
| `Alt+,/.` | Song top/end |
| `Backspace` / `Tab` | Clear cell/selection |
| `Alt+Backspace` | Clear full Song |

`B` changes assignment context only. `Alt+B` changes stored references. Plain horizontal
navigation owns edit Song-slot crossing.

## PHRASE CORE

| Key | Action |
|---|---|
| `1..4` | Select Phrase A/B/C/D |
| `Up/Down` | Capture/generation length `1/2/4/8` |
| `Left/Right` | Preview saved Phrase bar |
| `Ctrl+Left/Right` | Move visible `TO:` +/-1 row |
| `Ctrl+Up/Down` | Move visible `TO:` +/-8 rows |
| `R` / `Shift+R` | Next / previous capture role |
| `P` | Cycle derive parent |
| `Enter` | Capture current Song region |
| `D` | Derive parent into selected slot |
| `G` | Generate fresh connected Phrase at `TO:` |
| `W` | INSERT saved Phrase before `TO:`; shift following rows |
| `Alt+W` | REPLACE Phrase lanes at `TO:`; no row shift |
| `Backspace` / `Delete` | Clear selected Phrase |

`G` is STOP-only for multi-row generation. While PLAY is active it shows
`STOP PLAYBACK FOR PHRASE` and does not stop/restart transport automatically. Successful
`G` and `W` advance `TO:` by the Phrase length. Occupied Phrase lanes reject fresh `G`
without shifting/overwriting; non-Phrase Song lanes are preserved.

Cardputer punctuation positions normalize to arrow HID keys, so use `Ctrl+Arrow` for
`TO:` rather than raw comma/period shortcuts.

## OVERVIEW / SEQUENCER HUB

### Normal groovebox overview

| Key | Action |
|---|---|
| `Up/Down` | Select track |
| `Left/Right` | Select step |
| `Fn+Left/Right` | Selected-track volume -/+ |
| `-` / `=` | Track volume compatibility alias |
| `X` | Toggle hit/note |
| `A` | Toggle accent |
| `Enter` | Open track detail |
| `Space` | Transport |
| `Q..I` | Select local pattern |
| `B` | Toggle pattern bank |
| `Ctrl+C/V` | Copy / Paste |

### HUB MIDI

Open from MIDI Player with `H` after a file is loaded.

| Key | Action |
|---|---|
| `H` / `Esc` | Return to MIDI Player |
| `Up/Down` | Select projected physical layer |
| `Left/Right` | Change route immediately `AUTO <-> CH1..CH10` |
| `Fn+Left/Right` | Selected physical-track level +/-5% |
| `Enter` | Mute/unmute selected layer |
| `1..9` | Mute/unmute physical tracks directly |
| `S` | Solo selected layer |
| `A` | All MIDI tracks on |
| `Space` | MIDI transport |
| `C` | Reminder: route is edited with Left/Right |

Route changes work during PLAY and persist immediately per matching file identity.
There is no pause-first or Enter-to-commit route mode. Explicit destinations require
SEQTRAK routing; RAW routing passes source channels through.

## PROJECT / SETUP

| Key | Action |
|---|---|
| `Tab` | Next section; MIDI browser can open import matrix |
| `Up/Down` | Select row/file |
| `Left/Right` | Adjust value/dialog focus |
| `Enter` | Open/activate |
| `G` | Jump to GENRE |
| `Esc` / `Backspace` | Close dialog/go up directory |
| `X` | Delete selected supported scene/file |

## MIDI PLAYER

| Key | Action |
|---|---|
| `Enter` | Open selected MIDI file |
| `Space` | MIDI transport |
| `H` | Open HUB MIDI / return to Player |
| `1..9` | Physical-track mute |
| `U` | Physical-track mute mixer |
| `I` | Channel inspector |
| `S` | Structural inspector |
| `D` | Performance/throughput panel |
| `B` / `Backspace` | Files/previous panel |
| `Arrows` | Select/seek/scroll/adjust according to panel |
| `C` | Clock source |
| `T` | Tempo mode |
| `M` | RAW / SEQTRAK routing |
| `G` | Groove transport/follow |
| `R` | Restart file |
| `V` | Velocity boost |
| `X` | Panic SMF-owned notes |

`Alt+H` always opens help; unmodified `H` remains Player <-> HUB MIDI navigation.
