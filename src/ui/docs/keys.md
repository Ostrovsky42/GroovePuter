# GroovePuter 0.9.14 Key Map — Cardputer ADV

This is the canonical external key reference for the current 0.9.14 development runtime. `Alt+H`
opens page-aware on-device help; this file is the fuller release reference.

## Workflows

```text
PERFORM:  MIDI KEYBOARD -> MIDI PLAYER
GENERATE: GENRE -> FEEL
HUB:      OVERVIEW -> SYNTH A -> SYNTH B -> DRUMS
SONG:     SONG -> MATERIAL -> MATERIAL BANK
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
| `Alt/Fn+1..0` | Direct page jump, see below |
| `Space` | Active transport unless the page consumes it; while following MIDI IN, mute/unmute GroovePuter |
| `Alt+P` | MIDI Player |
| `Alt+Y` | TEMPO: project BPM and MIDI clock source; also Fn+M -> TEMPO |
| `Alt+K` | SAMPLER |
| `Alt+V` | GENRE |
| `Alt+W` | Waveform overlay except Phrase REPLACE |
| `Alt+X` | LiveMix ON/OFF |
| `Alt+M` | Song mode ON/OFF |
| `Alt+\` | `CARBON <-> CYBER` |

The active page gets first refusal before global fallbacks.

`Alt/Fn+digit` jumps to one page per digit; `2` is PROJECT, the rest follow the
workflows left to right:

| Digit | Page | Digit | Page |
|---|---|---|---|
| `1` | GENRE | `6` | DRUMS |
| `2` | PROJECT | `7` | SONG |
| `3` | OVERVIEW | `8` | MATERIAL |
| `4` | SYNTH A | `9` | MIDI KEYBOARD |
| `5` | SYNTH B | `0` | MIDI PLAYER |

PROJECT also opens from `Fn+M`. FEEL has no digit: `]` from GENRE. MATERIAL BANK has no direct key yet (open UI
issue: `[`/`]` are Tab-only peers on SONG and MATERIAL). On MIDI
PLAYER, `Fn+1..9` mute tracks, so use `Alt+digit` to leave it.

## MIDI KEYBOARD / PERFORM

| Key | Action |
|---|---|
| `QWERTYUIOP` | Upper scale-aware manual |
| `ASDFGHJKL` | Lower manual |
| `N` | NOTE mode ON/OFF |
| `\` | Cycle output target |
| `,` / `.` | Previous / next scale on SDL/external keyboard; Cardputer uses PERFORMANCE TOOLS -> KEY -> SCALE with Left/Right |
| `-` / `=` | Octave down / up |
| `Tab` | Open PERFORMANCE TOOLS (or cycle context when open) |

### PERFORMANCE TOOLS

Contextual layer: a fixed tab bar `[KEY] CHORD ARP RHYTHM`, one parameter per
row, and a hint line at the bottom that says what `Left/Right` and `Enter` do
for the selected row. Each context remembers its own selected row. Output
target and MONO/POLY receiver mode are rows in KEY.

| Key | Action |
|---|---|
| `Tab` / `Backspace` | Next / previous context (KEY -> CHORD -> ARP -> RHYTHM) |
| `Up` / `Down` | Select parameter row |
| `Left` / `Right` | Decrease / increase selected value |
| `Enter` | Toggle or secondary action for the row (see hint line) |
| `Esc` / `` ` `` | Return to live PERFORM |

The `; , . /` characters are swallowed while the layer is open: on the
Cardputer those keys are the arrow keycaps and arrive together with the arrow
scancode.

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
| `Alt+R` / `Opt` | Source STEPS <-> MELODY; with no Melody yet, makes one from the steps first. `Opt` (Cardputer) works from any synth tab |
| `Alt+N` | New empty Melody in the current slot; its steps are replaced only on `Alt+Enter`; `Ctrl+Z` steps back; unsaved Melody edits block it |
| `Q..I` on MELODY | Jump to that slot's accepted Melody; a slot with steps only gets a new empty Melody (its steps stay until `Alt+Enter`); unsaved edits -> `ALT+ENTER SAVE` |
| `B` on MELODY | Same slot position in the other bank, with the same rules |
| `Ctrl+Left/Right` on MELODY | Cursor to previous / next bar |
| `C` in NOTE ENTRY | Repeat the last entered pitch on the current step |
| `F` | Toggle audible step Retrig (starts at R2) |
| `Alt+Up/Down` | Retrig count 1..8 when Retrig is active |
| `G` on STEPS | Genre/recipe/rhythm/STYLE/harmony generation of the selected synth, NOTE ENTRY OFF |
| `P` on STEPS | Cycle shared STYLE: FAITHFUL -> VARIANT -> REWORK; affects the next G, NOTE ENTRY OFF |
| `Alt+G` on STEPS | Legacy genre-based generator of the selected synth; does not use shared STYLE |
| `Alt+Enter` / `Ctrl+Enter` | Accept working material |
| `Alt+Backspace` / `Alt+X` | Discard working edits to accepted material |
| `Enter` with NEXT ready | Request GO; while playing, activate at the next bar |
| `Esc` with NEXT ready | Cancel NEXT, or disarm queued GO while keeping NEXT |
| `Ctrl+C/V` | Copy / Paste |

On the Melody editor (`SOURCE: MELODY`), `Left/Right` move along time and `Up/Down`
move the note to the next note of the project key shown as `KEY C DOR` in the
header (a note outside the key goes to the nearest key note); `Ctrl+Up/Down`
move it by exactly one semitone. `K` raises the key's tonic a semitone and `M`
picks the next scale; notes stay where they are, and G generates in the same
key. `V` switches to list view. `Enter` adds a note,
`Backspace` deletes the note at the cursor, `Alt+Left/Right` shortens/lengthens it,
`J` joins it to the next note, and `G` changes the grid. `Ctrl+Left/Right` jump
to the previous/next bar. `[` / `]` switch the workflow page as on every page;
`Q..I` and `B` pick a saved Melody;
`Alt+Up/Down` scroll the
roll's pitch window freely (held, it repeats; elsewhere modified keys never
repeat); it comes back to the selected note when that note changes. The left
column names the top and bottom rows and every C; `^3` / `v2` count the notes
of the bar above / below the window.
`L` / `Alt+L` change Melody length when the added/removed bar does not truncate a
note. `Ctrl+Z` undoes a retained note edit.

The STEPS letters work on the Melody too: `A`/`Z` move the note one key note
up/down, `S`/`X` an octave, `Alt+A` toggles its accent.

Chords: `H` on a single note builds the project key's triad on it in one press;
`H` on a chord adds one more tone on top (a triad becomes a seventh chord). `C`
cycles the notes of the chord in the cursor cell, low to high, and the pitch
keys, `Backspace`, `Alt+A` and `Alt+Left/Right` then act on that note. `Alt+C`
turns the chord into an arpeggio: its notes one per grid step, low to high,
every note at least once and around again while a longer chord lasts; it needs
that many free steps before the next note. `Ctrl+Z` brings the chord back. Keys pressed together on an
external keyboard (within 40 ms) are recorded as one chord on the cursor cell.
Chords play on SEQTRAK over MIDI; the internal synth stays mono and plays the
chord's top note (an arpeggio plays everywhere). Thin lines separate the pitch
rows; the dotted one is the key's tonic.

In NOTE ENTRY, repeating or holding the same pitch can extend the note into the
next step as a continuation (shown as `TI`), rather than entering a new attack.

In STEPS outside NOTE ENTRY, plain `G` uses the active Genre/Variant/Rhythm/P-level/harmony identity. During PLAY
the selected lane publishes at `BAR_START`; the other synth and drums stay unchanged.
Inside NOTE ENTRY, `G` and `P` remain note input (including scancode-only events).
`P` only changes the shared request selector (P1/P2/P3 internally); it does not regenerate,
change the current pattern, or replace Undo. DRUMS and MATERIAL see the same selector.
`Alt+G` retains the legacy algorithm and the same selected-synth scope; it is not
whole-scene CHAOS. In STOP its runtime events update immediately; in PLAY the old
runtime remains audible until `BAR_START`, just as for G.
Undo before that boundary cancels the pending activation; Redo during PLAY and
Undo after activation require STOP (`UNDO/REDO: STOP OR WAIT`).

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

Lane labels include their direct mute keys. Default mapping is `3KIK 4SNR 5HH1 6HH2 7PR1 8PR2 9RIM 0CLP`; SP12 follows its global swap as `0RIM 9CLP`. Accent is shown on the individual hit cell; there is no separate aggregate ACC row.

| Key | Action |
|---|---|
| `Tab` | Sequencer / automation subpage |
| `Q..I` | Pattern 1..8 |
| `B` | Toggle bank A/B |
| `Alt+[` / `Alt+]` | Previous / next pattern page |
| `Arrows` | Move grid cursor |
| `Enter` | Toggle selected hit |
| `A` | Toggle accent on the selected existing hit only |
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

For Synth A/B, a Song row refers to a slot. The slot's saved descriptor selects
Pattern or Melody playback; the row does not store a separate type. Accepted Melody
must be saved in its slot for this assignment to play as Melody after reload.

`B` changes assignment context only. `Alt+B` changes stored references. Song-slot
crossing and the visible PAT assignment bank are independent controls.

Song editing distinguishes Song-generated material from manual/imported material.
Clearing a Song cell removes its arrangement reference; an unreferenced Song-generated
orphan may be reused by later Song generation, while non-empty manual/imported patterns
are never reclaimed automatically. A resident page still contains 16 slots per track;
if all of them are legitimately referenced/manual, use `Alt+]` to move to another
pattern page instead of clearing the project. Song stores page-aware global pattern IDs,
so playback can return to the required page through the existing deferred page-switch
path.

## MATERIAL

Generated-Phrase product workflow. The request (`LENGTH`/`STYLE`/`TO`) and
`LAST ACCEPTED` (`BAR`/activity) are separate objects, never one shared
timeline.

| Key | Action |
|---|---|
| `Up/Down` | Move focus `LENGTH -> STYLE -> TO [-> BAR]` (`BAR` only when a live accepted Phrase exists) |
| `Left/Right` | Adjust the focused field (length 1/2/4/8, style, TO placement, or accepted bar) |
| `Enter` (focus `TO`) | `EXPLICIT` row -> `APPEND` (no effect while already `APPEND`) |
| `Enter` (focus `BAR`) | Focus the accepted bar's Song/pattern context (STOP-only) |
| `G` | Generate into the resolved `TO` row |
| `P` | Cycle `STYLE` (same value as the focused STYLE field) |
| `D` | Grow an eligible fresh TAKE (see below) |
| `R` | Make room: reuse unused takes (plain `R` only; Alt/Ctrl/Meta+R are other shortcuts) |

`TO` always shows the row `G` would actually target right now: `APPEND` resolves
against the Song's current logical end every frame, or `EXPLICIT` if entering
MATERIAL from SONG with `Alt+J`, or after moving `TO` manually. Admissibility
(`FREE`/`OCCUPIED`/`NO ROOM`) mirrors the exact generation-availability check.
`LAST ACCEPTED` is retrospective only and disappears (`LAST --`) if its
generated material is no longer structurally present in the Song.

For the bounded musical-play cycle, press `G` for a new TAKE on the defaults
(`LENGTH 4B`, `STYLE REWORK`), then `D` while its source remains unedited and
unchanged. The result is `DEVELOP + BREAK 8B` or `BREAK ONLY 4B`; playback
changes at the next bar boundary. `Ctrl+Z` removes the added cycle in one step.
Non-4-bar, edited, VARIANT/FAITHFUL Takes are refused; changing STYLE after making
the TAKE does not convert it. Acid and House do not grow at all: `D` says
`<GENRE> CAN'T GROW: FN+M GENRE`, since another TAKE in the same genre cannot
help. On `MATERIAL BANK`, `D` still means derive.

### Make room (MATERIAL, `R`)

When `G` or `D` says `NO ROOM`, the page has no run of consecutive free pattern slots. Deleting Song
rows removes only the Song reference; the generated patterns stay in their slots. Press plain `R`:

| Key | Action |
|---|---|
| `R` | Ask: `REUSE n UNUSED TAKES?` (takes generated in this session, not in Song, not edited by you) |
| `Enter` | Yes: they will be replaced by the next `G`/`D`; you return to MATERIAL (`ROOM FOR 4B: PRESS G`) |
| `Esc` / `R` | No, back |
| `S` | Choose slots yourself (the slot-by-slot view below) |

The answer is one sentence: only takes made in this session that you have not edited, and that nothing
uses, are offered. Older material, hand-made or edited patterns, anything in Song, the CURRENT selection or
the live Undo are **never** offered automatically. Nothing is erased when you say yes; the next generation
uses free slots first and replaces these only when nothing else fits. **After a replacement Undo restores
the Song rows but not the old content.** The product line reads `NO SLOTS: R REUSE n` when there is something
to offer and `REPLACES ALLOWED` when `G` will use allowed slots. Permissions are not saved: they end when a
scene is loaded, a new scene is made or the page changes, and an edit cancels the permission of its slot.

#### Choosing slots yourself (`R`, then `S`)

| Key | Action |
|---|---|
| `Left/Right` | Move one slot |
| `Up/Down` | Move one bank (8 slots) |
| `Enter` on `~` | Allow replacement (asks to confirm the first time) |
| `Enter` on `*` | Cancel the permission |
| `Enter` on a held slot | Names what holds it; nothing changes |
| `R` / `Esc` | Back to MATERIAL |

Grid characters: `.` free, `~` unused (may be allowed), `*` allowed, letters = held and therefore protected:
`S` Song row, `P` Phrase Bank, `C` CURRENT, `W` working edit, `N` queued NEXT, `M` Melody, `U` live Undo.
`SLOT SPACE: NOW / AFTER ALLOWED` shows, for TAKE at the selected LENGTH and for GROW 4B/8B, whether a
consecutive run exists now and after the allowed replacement; it says nothing about whether GROW will run
(style, edits and an already published cycle are reported by `D`).

## MATERIAL BANK

Capture/derive/write workspace. Its own `TO:` destination and generation length are
independent of the MATERIAL product request above.

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

## TEMPO panel

Open with `Alt+Y` or `Fn+M -> TEMPO`. BPM is focused on open:
`Left/Right` changes it by 1, `Alt+Left/Right` by 5 (10–250 BPM).

Holding Left/Right speeds up (1 -> 4 BPM per repeat).

`Y` switches CLOCK between INTERNAL and MIDI IN from anywhere in the panel
(`Down` + `Left/Right` or `Enter` does the same on the CLOCK row).
MIDI IN follows MIDI Clock from any USB MIDI device or DAW: BPM becomes
read-only and Play/Stop come from that device. The panel shows WAITING,
SYNCING, IN SYNC, CLOCK HOLD or CLOCK LOST. Without a valid incoming clock
BPM reads `--.-`; after HOLD/LOST it shows the LAST BPM.

While following MIDI IN, `Space` on any page mutes or unmutes GroovePuter
(its audio, Pattern MIDI and MIDI Player notes) without stopping it: it keeps
following, so unmuting comes back on the beat. A new Play from the other
device clears the mute; Continue keeps it. On the MIDI Player, `Space` still
arms a file that is not playing; an armed file joins a running master on the
next bar.

Settings DEVICE contains Theme and Main Volume. Manual Groove Mode/Flavor
controls have been removed from settings; saved project fields are preserved.
