# GroovePuter 0.9.2 Key Map — Cardputer ADV

This is the canonical external key reference for the current 0.9.2 hardening runtime. `Alt+H`
opens page-aware on-device help; this file is the fuller release reference.

## Workflows

```text
PERFORM:  MIDI KEYBOARD -> MIDI PLAYER
GENERATE: GENRE -> FEEL
HUB:      OVERVIEW -> SYNTH A -> SYNTH B -> DRUMS
SONG:     SONG -> PHRASE -> PHRASE CORE
SETTINGS: PROJECT / SETUP
```

There are **12 active pages**. Persisted `GENERATION` and `TEXTURE` IDs resolve to
FEEL. Persisted standalone Synth SOUND IDs resolve to their owning Synth A/B page;
sound editing lives in local `NOTES -> KNOBS -> MORE` tabs.

## Global navigation

| Key | Action |
|---|---|
| `Alt+H` | Toggle page-aware help |
| `Ctrl+Z` | Undo last retained Pattern / Song / Phrase edit |
| `Fn+M` | Workspace launcher |
| `Fn+Tab` / `Fn+Shift+Tab` | Next / previous workflow |
| `[` / `]` | Previous / next page inside workflow |
| `Fn+[` / `Fn+]` | Previous / next workflow |
| `Alt+[` / `Alt+]` | Previous / next pattern page |
| `Alt/Fn+1..0` | Direct page jump |
| `Space` | Active transport unless the page consumes it |
| `Alt+P` | MIDI Player |
| `Alt+K` | SAMPLER |
| `Alt+V` | GENRE |
| `Alt+W` | Waveform overlay except Phrase REPLACE |
| `Alt+X` | LiveMix ON/OFF |
| `Alt+M` | Song mode ON/OFF |
| `Alt+\` | `CARBON <-> CYBER` |

The active page gets first refusal before global fallbacks.

## MIDI KEYBOARD / PERFORM

| Key | Action |
|---|---|
| `QWERTYUIOP` | Upper scale-aware manual |
| `ASDFGHJKL` | Lower manual |
| `N` | NOTE mode ON/OFF |
| `\` | Cycle output target |
| `,` / `.` | Previous / next scale |
| `-` / `=` | Octave down / up |
| `Tab` | Open/close PERFORMANCE TOOLS |

### PERFORMANCE TOOLS

| Key | Action |
|---|---|
| `1` | ARPEGGIATOR |
| `2` | DIRECTION |
| `3` | CHORD |
| `4` | MEMORY |
| `5` | STRUM |
| `6` | RATCHET |
| `7` | EUCLIDEAN |
| `8` | ROTATE |
| `9` | Receiver `MONO/POLY` |
| `-` / `_` | Velocity -10 |
| `=` / `+` | Velocity +10 |

Performance velocity is bounded to `10..120`. Receiver MONO/POLY is external-MIDI
ownership; internal Synth A/B remain sequencer/pattern instruments.

## GENRE 1/2

| Key | Action |
|---|---|
| `Tab` / `Up/Down` | Select Genre/Variant/Rhythm/Apply field |
| `Left/Right` | Adjust selected field |
| `Enter` | Apply selected policy |
| `G` | Explicit full Stage 15 generation |
| `P` | `P1 CANON -> P2 VAR -> P3 TRANS` |
| `M` | Cycle `PROFILE`, `MATERIALIZE`, `MATERIALIZE+BPM` |

During PLAY, accepted full generation publishes at the next real `BAR_START`; while
stopped it commits immediately. Repeated accepted `G` rerolls the same selected
musical identity through the bounded session attempt stream.

## FEEL 2/2

| Key | Action |
|---|---|
| `Tab` / `Up/Down` | Select FEEL field |
| `Left/Right` | Adjust selected value |
| hold `Left/Right` | Accelerated adjustment |
| `Enter` / `Space` on PRESET | Apply selected FEEL preset |
| `P` | Cycle shared P1/P2/P3 request level |

FEEL owns timing and velocity only: profile, swing, bounded feel amount, velocity
variation, repeat cycle `1/2/4/8`, and presets.

## SYNTH A / SYNTH B

`Tab` cycles `NOTES -> KNOBS -> MORE`.

### NOTES

| Key | Action |
|---|---|
| `Q..I` | Pattern slot 1..8 outside NOTE ENTRY |
| `B` | Toggle bank A/B |
| `Alt+[` / `Alt+]` | Previous / next pattern page |
| `Arrows` | Move step cursor |
| `N` | NOTE ENTRY ON/OFF |
| `G` | Reroll only the selected synth lane when NOTE ENTRY is OFF |
| `Ctrl+C/V` | Copy / Paste |

Plain `G` uses the active Genre/Variant/Rhythm/P-level/harmony identity. During PLAY
the selected lane publishes at `BAR_START`; the other synth and drums stay unchanged.
Inside NOTE ENTRY, `G` remains note input.

### KNOBS / MORE

| Key | Action |
|---|---|
| `Tab` | Cycle NOTES / KNOBS / MORE |
| `Left/Right` | Focus/change value |
| `Up/Down` | Adjust value/select row |
| `Ctrl+1..2` | Pattern bank A/B |
| `Q..I` | Pattern selection when NOTE mode is off |
| `A/Z S/X D/C F/V` | Quick parameter controls |
| `T/G` | Oscillator +/- |
| `Y/H` | Filter type +/- |
| `N/M` | Distortion / Delay |
| `Ctrl+A/X/C/V` | Reset Cutoff / Resonance / Env Amount / Env Decay |

## DRUMS

| Key | Action |
|---|---|
| `Tab` | Sequencer / automation subpage |
| `Q..I` | Pattern 1..8 |
| `B` | Toggle bank A/B |
| `Alt+[` / `Alt+]` | Previous / next pattern page |
| `Arrows` | Move grid cursor |
| `Enter` | Toggle hit |
| `A` | Toggle accent |
| `G` | Drums-only strong generation at current P-level |
| `Ctrl+G` | Randomize focused drum voice |
| `Alt+G` | Full-pattern CHAOS |
| `Ctrl+Alt+G` | Stage 12 phrase audition/probe |
| `P` | Cycle shared P1/P2/P3 request level |
| `Ctrl+C/V` | Copy / Paste |

## SONG

| Key | Action |
|---|---|
| `Left/Right` | Move `Synth A -> Synth B -> Drums`; crossing the outer edge changes edit Song slot A/B |
| `Up/Down` | Move Song row |
| `Enter` | Jump to referenced pattern editor |
| `Q..I` | Assign existing slot from visible `PAT:A/B` context |
| `G` | Generate safe material and assign selected cell; unique Song-generated material rerolls in place |
| double `G` | Materialize Synth A + Synth B + Drums for current row |
| `Alt+G` | Generate selected area |
| `Ctrl+G` | Cycle Song generator mode |
| `Backspace` | Clear current cell / selected Song cells |
| `B` | Toggle visible `PAT:A/B` assignment bank |
| `Alt+B` | Flip stored-reference/selection bank |
| `Ctrl+B` | Play Song slot A/B |
| `Alt+[` / `Alt+]` | Previous / next pattern page when the resident 16-slot page is full |
| `Ctrl+N` / `Ctrl+M` | Insert / remove row |
| `V` | Toggle DR/VO lane |
| `X` | Split compare |
| `L` | Loop-lock around playhead |
| `Ctrl+L` | Loop mode |
| `Ctrl+R` | Reverse playback |
| `Alt+X` | LiveMix ON/OFF |
| `Ctrl+C/V` | Copy / Paste |
| `P` | Cursor to playhead |
| `Alt+J` | Jump to PHRASE with this row as the explicit `TO` destination |

`B` changes assignment context only. `Alt+B` changes stored references. Song-slot
crossing and the visible PAT assignment bank are independent controls.

0.9.2 hardening distinguishes Song-generated material from manual/imported material.
Clearing a Song cell removes its arrangement reference; an unreferenced Song-generated
orphan may be reused by later Song generation, while non-empty manual/imported patterns
are never reclaimed automatically. A resident page still contains 16 slots per track;
if all of them are legitimately referenced/manual, use `Alt+]` to move to another
pattern page instead of clearing the project. Song stores page-aware global pattern IDs,
so playback can return to the required page through the existing deferred page-switch
path.

## PHRASE

Generated-Phrase product workflow. `NEXT REQUEST` (`LENGTH`/`DEPTH`/`TO`) and
`LAST ACCEPTED` (`BAR`/activity) are separate objects, never one shared
timeline.

| Key | Action |
|---|---|
| `Up/Down` | Move focus `LENGTH -> DEPTH -> TO [-> BAR]` (`BAR` only when a live accepted Phrase exists) |
| `Left/Right` | Adjust the focused field (length 1/2/4/8, depth, TO placement, or accepted bar) |
| `Enter` (focus `TO`) | `EXPLICIT` row -> `APPEND` (no effect while already `APPEND`) |
| `Enter` (focus `BAR`) | Focus the accepted bar's Song/pattern context (STOP-only) |
| `G` | Generate into the resolved `TO` row |
| `P` | Cycle `DEPTH` (shortcut, same owner as focused `DEPTH`) |

`TO` always shows the row `G` would actually target right now: `APPEND` resolves
against the Song's current logical end every frame, or `EXPLICIT` if entering
PHRASE from SONG with `Alt+J`, or after moving `TO` manually. Admissibility
(`FREE`/`OCCUPIED`/`NO ROOM`) mirrors the exact generation-availability check.
`LAST ACCEPTED` is retrospective only and disappears (`LAST --`) if its
generated material is no longer structurally present in the Song.

## PHRASE CORE

Legacy capture/derive/write workspace, now a separate page from PHRASE. Its
own `TO:` destination and generation length are independent of the PHRASE
product request above.

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
| `W` | INSERT saved Phrase before `TO:` and shift following rows |
| `Alt+W` | REPLACE Phrase lanes at `TO:` without row shift |

Fresh multi-row Phrase generation is STOP-only. During PLAY it reports
`STOP PLAYBACK FOR PHRASE`; successful `G` or `W` advances `TO:` by Phrase length.

## OVERVIEW / SEQUENCER HUB

### Normal overview

| Key | Action |
|---|---|
| `Up/Down` | Select track |
| `Left/Right` | Select step |
| `Fn+Left/Right` | Selected-track volume -/+ |
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

Route changes work during PLAY and persist immediately per matching file identity.
There is no pause-first or Enter-to-commit route mode. RAW routing keeps source
channels and therefore does not accept explicit SEQTRAK destination overrides.

## PROJECT / SETUP

| Key | Action |
|---|---|
| `Tab` | Next section; MIDI browser can open import matrix |
| `Up/Down` | Select row/file |
| `Left/Right` | Adjust value/dialog focus |
| `Enter` | Open/activate |
| `G` | Jump to GENRE |
| `Esc` / `Backspace` | Close dialog/go up directory |
