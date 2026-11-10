# GroovePuter 0.9.21 (unreleased) Key Map — Cardputer ADV

This is the canonical external key reference for the 0.9.21 (unreleased) development source; the last tagged release is v0.9.20. v0.9.16 is the earlier hardware-accepted foundation. `Alt+H` opens page-aware on-device help; this file is the full expert reference.

## First Five Minutes

You do not need the whole table below before making music. Start with these rules:

| Key | Beginner meaning |
|---|---|
| `Space` | Play / Stop |
| `Fn+M` | Workspace launcher; choose GENRE or MATERIAL |
| `G` | Generate the thing you are looking at |
| `D` on MATERIAL | DEVELOP the fresh TAKE |
| `Ctrl+Z` | Undo the last retained Pattern / Song / Phrase edit |
| `Alt+H` | Help for the current page |

These are beginner rules, not global overrides. In particular, `D` means DEVELOP in this first-session vocabulary only on MATERIAL; MATERIAL BANK keeps `D` as derive and Synth pages keep their own development/edit semantics.

`Alt+V` opens GENRE directly.

For the complete first-session walkthrough see [`../../docs/user/QUICKSTART.md`](../../docs/user/QUICKSTART.md).

## Workflows

```text
PERFORM:  MIDI KEYBOARD -> MIDI PLAYER
GENERATE: GENRE -> FEEL
HUB:      OVERVIEW -> SYNTH A -> SYNTH B -> DRUMS
SONG:     SONG -> MATERIAL -> MATERIAL BANK
SETTINGS: PROJECT / SETUP
```

GEN is a local panel inside GENRE, not a third WorkflowPages destination. `Tab` opens it; `Tab` again moves to FEEL.

There are **12 active pages**. Persisted `GENERATION` and `TEXTURE` IDs resolve to
FEEL. Persisted standalone Synth SOUND IDs resolve to their owning Synth A/B page;
sound editing lives in local `NOTES -> KNOBS -> MORE` tabs.

## Global navigation

| Key | Action |
|---|---|
| `Alt+H` | Toggle page-aware help |
| `Ctrl+Z` | Undo last retained Pattern / Song / Phrase edit. Single level: it takes back only the last key press (one STEP SETTINGS change), not the whole series |
| `Fn+M` | Workspace launcher |
| `Fn+Tab` | Next workflow |
| `[` / `]` | Previous / next page inside workflow |
| `Fn+[` / `Fn+]` | Previous / next workflow |
| `Alt+[` / `Alt+]` | Previous / next pattern page |
| `Alt/Fn+1..0` | Direct page jump, see below |
| `Space` | Active transport unless the page consumes it; while following MIDI IN, mute/unmute GroovePuter |
| `Alt+P` | MIDI Player |
| `Alt+Y` | TEMPO: project BPM and MIDI clock source; also Fn+M -> TEMPO |
| `Ctrl+=` / `Ctrl+-` | BPM +1 / -1 (`Alt+=` / `Alt+-` by 5), toast `BPM 128`; a page that owns the key keeps it (HUB volume, PERFORM octave) |
| `Alt+K` | SAMPLER |
| `Alt+V` | GENRE |
| `Alt+W` | Waveform overlay except Phrase REPLACE |
| `Alt+X` | LiveMix ON/OFF |
| `Alt+M` | Song mode ON/OFF |
| `Alt+\` | Theme: `CLEAN` <-> `CYBER` (the key shows the name) |

Hard-global Alt shortcuts are handled before page-local input. Outside those reserved chords, the active page gets first refusal before late global fallbacks.

`Alt/Fn+digit` jumps to one page per digit; `2` is PROJECT, the rest follow the
workflows left to right:

| Digit | Page | Digit | Page |
|---|---|---|---|
| `1` | GENRE | `6` | DRUMS |
| `2` | PROJECT | `7` | SONG |
| `3` | OVERVIEW | `8` | MATERIAL |
| `4` | SYNTH A | `9` | MIDI KEYBOARD |
| `5` | SYNTH B | `0` | MIDI PLAYER |

PROJECT also opens from `Fn+M`. FEEL has no digit: `]` from GENRE. MATERIAL BANK has no digit: `]` from
MATERIAL (SONG -> MATERIAL -> MATERIAL BANK). On MIDI
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
| `Z` / `C` / `V` / `B` / `Del` | LOOPER: REC-DUB / PLAY-STOP / UNDO / HOLD / CLEAR (see LOOPER below) |
| `M` | LOOPER: active layer 1 / 2 (REC-DUB, HOLD, CLEAR, MUTE act on it) |
| `X` | Panic: live notes off, HELPER silenced, the LOOPER stops (its notes are kept) |
| `Ctrl+Z` | LOOPER undo while it has something to undo (it takes the key first); otherwise the usual `UNDO: EMPTY / NOT HERE` |

### LOOPER

A MIDI looper on the live PERFORM surface records KEYS (pressed notes before
Smart Play) or SOUND (the generated live notes) on a 1/16 grid into one loop of up to 192 notes
and played back on `LOOP CH`: `DX ROUTE` (the channel and target of PERFORM DX, the default) or `CH1..CH16`. The loop sounds over MIDI only, there is no internal sound; the first `Z` on the internal output shows a one-time toast `LOOP: DX ROUTE, NO INTERNAL SOUND` (hint `DX ROUTE: SAME CH AS PERFORM DX`). When the keyboard target is DRUMS, `Z` refuses with `LOOP: N/A FOR DRUMS`. Line 2 of the live surface shows the state, the active layer (`L1` /
`L2`, filled = muted), the pass with its notes (layer 1 ticks on top, layer 2
below, the active layer brighter), the playhead and `N/192` (brighter when `V` has something to undo, `FULL` at 192; `Z REC` on an empty loop in IDLE); at 8 bars only the bar marks are drawn, bar 5 taller.
Overlapping notes and chords play together over MIDI.
Settings in PERFORMANCE TOOLS -> INPUT, group `--- LOOPER ---` (between
MIDI IN and HELPER): `LOOPER` (Enter = `Z`), `REC` KEYS/SOUND, `LOOP BARS` 1/2/4/8, `LOOP CH`,
`NOTE LENGTH` 100/75/50/25 %, `HOLD LAYER` (= `B`), `LAYER` (Enter = `M`),
`MUTE LAYER`, `TO MELODY`. Session only, never saved (neither the LOOPER nor the HELPER settings).

`REC` chooses KEYS (the notes pressed, before Smart Play) or SOUND (the notes generated by live playing, including chords and arp). Change it before arming or recording; the choice is locked during ARM/REC/DUB.

Both modes quantize note starts and ends to the 1/16 grid; recorded notes last at least one step. SOUND does not preserve sub-step ratchets, short gates, swing, or strum timing exactly: attacks can merge or become simultaneous. The 192-event pool is shared by both layers and undo, so an eight-bar 1/16 arp leaves room for 64 further events; additional notes are dropped with a FULL notice.
Playback can schedule up to 32 simultaneous notes per plan; denser same-step chords may lose notes. TO MELODY accepts up to 128 events and refuses larger exports.

`TO MELODY` (last row of the LOOPER group): `Left/Right` picks the target synth A or B (shown
as `SYN A`/`SYN B`; session only; until picked it follows GEN TARGET, which
the pick never changes). The first `Enter` asks `REPLACE MELODY A WITH LOOP? ENTER`;
a second `Enter` within 4 s replaces the Working Melody of that synth with the unmuted layers as played (pitch and length as
recorded, cut at the pass end, no Smart Play; held and hidden (undone) notes
are skipped). Changing the synth, the number of bars or the MUTE mask resets the confirmation.
If that synth holds an unsaved Working Melody the toast is `SAVE OR DISCARD A FIRST` and nothing is exported.
A third `Enter` does not open the synth page. On success the toast is `MELODY A <- LOOP  CTRL+Z ON SYNTH A`: one Undo step, taken back with `Ctrl+Z` on the synth page (NOTES tab; back to STEPS or the
previous Melody). Nothing is saved automatically: `Alt+Enter` on the synth page saves.

The LOOPER and the HELPER (the partner) share one MIDI reply player, so
they never play together: while `HELPER` is not OFF the LOOPER does not work, the `LOOPER` row shows `HELPER ON`, and `Z` and `C` show
`LOOP: HELPER OFF FIRST`. Turn it off at INPUT > HELPER > OFF. Turning HELPER on stops a sounding loop (`LOOP: STOPPED, HELPER ON`; its notes are
kept, `C` plays it again once HELPER is OFF).

Two layers share the 192 notes and one length. `M` picks the active layer (not
during the first REC pass; it ends a DUB first). `Z` records and dubs into the
active layer only: a note never replaces or hides a note of the other layer.
`B` (or the `HOLD LAYER` row) holds the active layer, `Del` clears it (the other layer plays on),
`MUTE LAYER` silences it at once. `V` undoes the last unit, whatever layer it was in,
unless that layer is held.

`LOOP BARS` keeps the notes: longer repeats the loop to fill the new length
(1 -> 2 -> 4 -> 8), shorter cuts the notes past the new end; the pass goes on
from where it is and `V` takes the change back (bars too). Not during the first
REC pass, not while a layer is held, and not when the repeat would pass 192
notes (`LOOP: NO ROOM TO REPEAT`).

| State | `Z` | `C` | `V` | `B` | `Del` |
|---|---|---|---|---|---|
| IDLE | ARM | - | undo CLEAR | - | - |
| ARMED (waits for the next bar under PLAY, or for the first note) | cancel | cancel | - | - | - |
| RECORD | then DUB / then PLAY | stop, keep notes | cancel recording | HOLD after the pass | cancel recording |
| PLAY | DUB now | STOP (notes off now) | undo last pass | HOLD | CLEAR |
| OVERDUB | back to PLAY now | STOP | undo last pass | PLAY + HOLD | CLEAR |
| STOPPED | PLAY + DUB now | PLAY | undo last pass | HOLD | CLEAR |

- One undo level: the last RECORD pass, the last OVERDUB pass that had notes, or
  CLEAR (`V` after `Del` brings the loop back).
- `HOLD` keeps the active layer as it is (no REC, UNDO or CLEAR): `Z`, `Del`, `V`/`Ctrl+Z` of that layer's
  last unit and `BARS` show `LOOP: HOLD, B TO RELEASE`; `C`, `X` and `M` still work,
  and the other layer records and clears freely. `B` again releases it. The chip `HOLD` is shown on the live surface, the toast is `LOOP: L1 HOLD ON/OFF`, the footer `[B]HOLD OFF`. Not the same as ARP HOLD or CHORD HOLD.
- `Space` stop silences the loop at once; the next `Space` start resumes it on
  the project bar grid (the same place in the loop). Without PLAY the loop runs
  on the PERFORM tempo from the first recorded note.
- Leaving PERFORM ends a recording. Under PLAY the loop keeps playing on any
  page; without PLAY it stops.
- `X` stops the loop and keeps it; only `Del` clears it.

### PERFORMANCE TOOLS

Contextual layer: a fixed tab bar `[KEY] CHORD ARP RHYTHM INPUT` (five tabs), one parameter per
row, and a hint line at the bottom that says what `Left/Right` and `Enter` do
for the selected row. Each context remembers its own selected row. Output
target and MONO/POLY receiver mode are rows in KEY.

| Key | Action |
|---|---|
| `Tab` / `Backspace` | Next / previous context (KEY -> CHORD -> ARP -> RHYTHM -> INPUT) |
| `Up` / `Down` | Select parameter row |
| `Left` / `Right` | Decrease / increase selected value |
| `Enter` | Toggle or secondary action for the row; `CHORD MODE` opens a list |
| `Esc` / `` ` `` | Return to live PERFORM |

The `; , . /` characters are swallowed while the layer is open: on the
Cardputer those keys are the arrow keycaps and arrive together with the arrow
scancode.

Performance velocity is bounded to `10..120`. Receiver MONO/POLY is external-MIDI
ownership; internal Synth A/B remain sequencer/pattern instruments.

The scrollable INPUT list on PERFORM has three groups, top to bottom: `--- MIDI IN ---`,
`--- LOOPER ---`, `--- HELPER ---`. The heading lines are skipped by
`Up/Down` (the cursor never rests on them). A scroll bar on the right edge shows when the list is longer than the screen.

- MIDI IN: `MIDI IN` (external MIDI input ON/OFF), `LISTEN CH` (OMNI or
  CH1..16), `SEND TO` (PERFORM = the KEY tab target and tools, or SYN A/SYN B/
  DRUMS), `PLAY AS` (live-note role, new notes only), `LEAD STYLE` (with
  `PLAY AS` LEAD). Target and output live in KEY; the USB role is set on
  PROJECT > MIDI.
- LOOPER: `LOOPER`, `REC`, `LOOP BARS`, `LOOP CH`, `NOTE LENGTH`, `HOLD LAYER`, `LAYER`, `MUTE LAYER`, `TO MELODY`; see LOOPER above.
- HELPER (`HELPER`, `REPLY BARS`, `REPLY CH`, `FREEZE RIFF`, `VARIATION`, `NEW RIFF IF`): `HELPER` OFF/ANSWER/DUET/RIFF replies over MIDI to what you play
  (RIFF repeats a short riff built from your playing). `REPLY BARS` 1/2/4,
  `REPLY CH` (`DX ROUTE` = the PERFORM DX route, or CH1..16). With RIFF only:
  `FREEZE RIFF` (ON keeps the riff, OFF lets a new one in at the next bar),
  `VARIATION` EXACT/ENDING/DEVELOP/FREE, `NEW RIFF IF` 1 NOTE/3 NOTES/HALF
  (how much you play to start a new riff). Session only; replies go to
  external MIDI, so confirm the destination before turning HELPER on. While HELPER is not OFF the LOOPER is blocked (`LOOP: HELPER OFF FIRST`).

## Words on the screen

- `FEEL`: timing and strength of the hit (the FEEL page 3/3, and part G on DRUMS).
- `SOUND`: the timbre (the DRUMS tab "DRUM SOUND", SOUND on the synths).
- `HOLD`: holding a looper layer (`B`); not the same as ARP HOLD or CHORD HOLD.
- `HELPER`: the partner (reply, duet, RIFF; the old LOOP mode is now called RIFF).
- `DX ROUTE`: the loop plays on the channel and target of PERFORM DX.
- `P1`/`P2`/`P3`: the levels of the generation style.

## GENRE 1/3

| Key | Action |
|---|---|
| `Tab` / `Up/Down` | Select Genre/Variant/Rhythm/Apply field |
| `Left/Right` | Adjust selected field |
| `Enter` | Open `RECIPE` or `SCALE` list; otherwise apply selected policy |
| `G` | Explicit full Stage 15 generation |
| `P` | Cycle the request level `P1` -> `P2` -> `P3` (P1 faithful to the classic, canon and recipe; P2 variation, a recognisable pattern with limited changes; P3 rework, freer transformation, default since 0.9.17; only on P3 can MATERIAL `D` develop a phrase, on P2 it shows `P: SET P3, THEN G`) |
| `Left/Right` on KEY | Choose the project tonic; affects the next generation |
| `M` on KEY | Cycle the project scale; affects the next generation |
| `H` on KEY | Open the `GENRE/AUTO/GRAPH` harmony strategy list |
| `N` | Apply the next named preset (Classic Acid, Rave Acid, Synthwave, EBM, Detroit, Darksynth); sets Genre and Variant, then applies per the `APPLY` mode |
| `M` on other fields | Cycle the `APPLY` mode: `STYLE ONLY`, `NEW TAKE`, `NEW TAKE + TEMPO` |

During PLAY, accepted full generation publishes at the next real `BAR_START`; while
stopped it commits immediately. Repeated accepted `G` rerolls the same selected
musical identity through the bounded session attempt stream.

In a selector, `Up/Down` chooses an item, `Enter` confirms it, and `Esc` cancels.
On the KEY row, `Enter` opens the scale list; `Left/Right` changes the tonic directly.
GENRE/RECIPE/RHYTHM selections remain pending until the `APPLY` row is confirmed;
the project `KEY` is changed immediately and affects the next generation.
The harmony strategy selected with `H` also waits for `APPLY`.

## GEN 2/3

`Tab` from GENRE opens GEN, `Tab` again goes on to FEEL, `Esc` returns to GENRE.

| Field | Values | What it does |
|---|---|---|
| `TARGET` | `STEPS` / `MELODY A` / `MELODY B` | Where plain `G` writes. `MELODY A/B`: `G` on GENRE writes a phrase into that synth's Melody; `G` on a synth's STEPS writes that synth's Melody. The synth switches to its Melody first; drums and steps stay. |
| `DRUMS` | `NEW` / `KEEP` | With `TARGET STEPS`: `NEW` makes a whole new TAKE (drums and both synths); `KEEP` rebuilds only the two synths against the drums already in the slot. A Melody never touches the drums. Enter with APPLY NEW TAKE follows it too. |
| `LENGTH` | `1/2/4/8` bars | Phrase length for a Melody (`G`, `Alt+G`) and for the MATERIAL TAKE; the same value as MATERIAL `LENGTH`. |
| `LIVELY` | `CALM` / `NORMAL` / `LIVELY` | How busy Synth B comes out: the lead in melodic genres, the chords in chord genres (Reggae, TripHop). `CALM` prefers sparse cells and long chords, `LIVELY` dense lines and stabs; `NORMAL` is the genre as it is. |
| `NOTES` | `SHORT` / `MIXED` / `LONG` | How long generated Melody notes ring: the genre's gate, some held on to the next note (more before rests), or all legato. Steps have no note length. |

Settings live for the session.

## FEEL 3/3

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
| `-` / `=` in NOTE ENTRY | NOTE rows octave: 1-2 .. 5-6 (default 3-4). Steps hold C1..C7; MIDI gets the note as written, the internal synth folds notes above B4 down by octaves |
| `L` on STEPS | PAIR VIEW + PAIR PLAY (outside NOTE ENTRY, where `L` is a pitch): rows 1-2 are slot N as now, rows 3-4 are slot N+1 of the same synth, header `A 1+2`, footer `L:1 SLOT` (`L:PAIR` when off). The cursor and every edit (note, accent, slide, clear, paste) follow the row and act on that slot with their own Undo. Slot 8 has no N+1: rows 3-4 are empty and not editable. `G`, `Alt+G`, `Ctrl+Backspace` act on slot N only (refused on rows 3-4). Plain `L` also starts PAIR PLAY (below); `L` again stops it and turns the view off. The pair is per synth and sticky: leaving the page or switching to the other synth does not stop it, and re-entering shows its view |
| PAIR PLAY on STEPS | Started by `L` with the view: this voice plays slot N, then N+1 at the next bar line, and back, as a loop. While the transport plays, the view starts at once but the sound starts at the next bar line (footer `PAIR PLAY: WAIT NEXT BAR`, then `PAIR PLAY 1+2`); stopped, it starts at once. Pattern mode only; refused with Song on (`SONG OFF FIRST`), a Melody slot (`STEPS ONLY`), unsaved edits (`SAVE FIRST`) or a queued GO; the view stays on after a refusal. Stops on `L`, Stop, changing slot, Song on (not on leaving the page). The other synth's page shows `A:PAIR` / `B:PAIR` in the footer. The selected slot does not move. `Ctrl+L` is not used on STEPS |
| `D` | On MELODY: vary the focused bar in place (`1 ANSWER`, `2 SEQUENCE`, `3 SYNCOPATE`, `4 CONTOUR`, `5 OCTAVE`, `6` the `ORIGINAL` bar again); each press is one `Ctrl+Z` step. On STEPS NOTES outside NOTE ENTRY: DEVELOP (revoice the NEXT). With NOTE ENTRY on on STEPS, `D` is the pitch D. On KNOBS `D` is Env Amount + (MORE ignores it) |
| `Alt+R` / `Opt` | Source STEPS <-> MELODY. With no Melody yet, `Alt+R` (or `Opt`) copies the steps into a new Melody (toast `MELODY <- STEPS`); `Ctrl+Z` removes it and returns to STEPS. On SONG, `Opt` on a cell with no Melody refuses (`NO MELODY (ALT+N ON STEPS)`). `Opt` (Cardputer) works from any synth tab |
| `Alt+N` | New empty Melody in the current slot; its steps are replaced only on `Alt+Enter`; `Ctrl+Z` steps back; unsaved Melody edits block it |
| `Q..I` on MELODY | Jump to that slot's accepted Melody; a slot with steps only gets a new empty Melody (its steps stay until `Alt+Enter`); unsaved edits -> `ALT+ENTER SAVE` |
| `B` on MELODY | Same slot position in the other bank, with the same rules |
| `Ctrl+Left/Right` on MELODY | Cursor to previous / next bar |
| `Ctrl+A` on MELODY | Select all notes in the focused bar (`BAR n/m ALL`); `Up/Down`/`A/Z` (key), `Ctrl+Up/Down` (semitone), `S/X` (octave), `Alt+Left/Right` (length), `Alt+A` (accent), `Alt+S` (slide), `Alt+E` (`NOTE SETTINGS`), and `Backspace` (clear bar) act on the whole bar as one `Ctrl+Z` step. `Esc` / `` ` `` or moving the cursor drops the selection |
| `Alt+S` on MELODY | Toggle slide (`SL`) on the focused note (or uniformly across the selected bar) |
| `Alt+E` on MELODY | `NOTE SETTINGS` window for the focused note (or all notes in the selected bar when `Ctrl+A` is active, showing `MIXED` when values differ): `VEL` (1..127, step 5), `PROB` (0..100%, step 5), `SLIDE` (`OFF`/`ON`), `CHANCE` (`100%`/`80%`). `Backspace` resets the field, `Ctrl+Backspace` resets all fields; each step is one `Ctrl+Z` |
| `Ctrl+C` / `Ctrl+V` / `Ctrl+D` on MELODY | Copy the current bar / replace the target bar with it / duplicate into the next empty bar; one Undo. A note or chord crossing the bar edge is refused |
| `C` in NOTE ENTRY | Repeat the last entered pitch on the current step |
| `F` | Toggle audible step Retrig (starts at R2) |
| `Alt+Up/Down` | Retrig count 1..8 when Retrig is active |
| `Alt+E` on STEPS | STEP SETTINGS window for the cursor step (all selected steps when an area is selected), works with NOTE ENTRY on. `Up/Down` picks PROB (0..100, step 5), TIMING (-23..23 ticks, step 1), VEL (1..127, step 5) or CHANCE (100% / 80%: at 80% the note sounds with 80% probability, multiplied by PROB; with PROB below 100 a line like `80% =64%` shows the result, PROB 80 and CHANCE 80% give `80% =64%`; not a quiet note, formerly GHOST); `Left/Right` or `-`/`=` changes it; `Backspace` resets the selected field, `Ctrl+Backspace` all fields of the step (or of the selection); the window hint is `UP/DN FIELD  L/R VALUE  ENT/ESC CLOSE`, the footer `UP/DN:FIELD L/R:VALUE` and `ENT/ESC:CLOSE`; each key press is one Undo step (`Ctrl+Z` takes back only the last press). `Enter` or `Esc` closes; `Opt` and other keys are ignored while it is open. Not offered on a Melody. A field that differs from a plain step (PROB 100, TIMING 0, VEL 100, CHANCE 100%) shows a small square left of its name and an accent-coloured value; the default shows neither. A note cell with a 2x2 dot is a step whose PROB, VEL or CHANCE differs from the default. Values move to the next multiple of 5, so 100 is always reachable |
| `G` on STEPS | Genre/recipe/rhythm/STYLE/harmony generation of the selected synth, NOTE ENTRY OFF |
| `P` on STEPS | Cycle shared request level P1 -> P2 -> P3 (P1 faithful, P2 variation, P3 rework); affects the next G, NOTE ENTRY OFF |
| `Alt+G` on STEPS | Legacy genre-based generator of the selected synth; does not use shared STYLE |
| `Ctrl+Backspace` on STEPS | Clear the whole pattern of the selected synth, one Undo (toast `PATTERN CLEARED  CTRL+Z UNDO`); ignores the selection (an area is erased by `Backspace`, toast `SELECTION CLEARED`) |
| `Alt+O` | Cycle the synth output mode (toast `SYNTH A OUT:`) |
| `Alt+Enter` / `Ctrl+Enter` | Accept working material |
| `Alt+Backspace` | Discard working edits to accepted material |
| `Enter` with NEXT ready | Request GO; while playing, activate at the next bar |
| `Esc` with NEXT ready | Cancel NEXT, or disarm queued GO while keeping NEXT |
| `Ctrl+C/V` | Copy / Paste |

`Alt+X` is reserved globally for LiveMix and therefore is not a Synth DISCARD chord. Likewise, the current hard-global `Alt+V` route is not a reachable Synth CONNECT shortcut.

On the Melody editor (`SOURCE: MELODY`), `Left/Right` move along time and `Up/Down`
move the note to the next note of the project key shown as `KEY C DOR` in the
header (a note outside the key goes to the nearest key note); `Ctrl+Up/Down`
move it by exactly one semitone. `K` raises the key's tonic a semitone and `M`
picks the next scale; notes stay where they are, and G generates in the same
key. `V` switches to list view. `Enter` adds a note,
`Backspace` deletes the note at the cursor, `Alt+Left/Right` shortens/lengthens it,
`J` joins it to the next note, and `Aa` (`Ctrl+G` on SDL) changes the grid. `Ctrl+Left/Right` jump
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
up/down, `S`/`X` an octave, `Alt+A` toggles its accent, `Alt+S` toggles slide,
`Alt+E` opens `NOTE SETTINGS` (`VEL`, `PROB`, `SLIDE`, `CHANCE`), and `Ctrl+A`
selects all notes in the focused bar for batch editing.

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

`G` (or `Alt+G`) on the Melody generates a whole phrase from the genre, key and STYLE:
as many bars as MATERIAL LENGTH asks for (1, 2, 4 or 8), for this synth only,
replacing the Melody as one edit. Each press gives another phrase; `Ctrl+Z`
brings the previous Melody back. Steps, slots and Song rows are not touched.
GEN `LIVELY` and `NOTES` shape it. The grid step that `G` used to cycle is the
Cardputer `Aa` key tapped alone (`Ctrl+G` on the emulator).

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
| `Enter` on MORE `TYPE` | Open the synthesizer engine list; Up/Down selects, Enter applies, Esc cancels |
| `Enter` on `PRESET` | Open the current engine's sound presets; DUAL OSC also opens this from its MAIN Preset knob |
| `Ctrl+1..2` | Pattern bank A/B |
| `Q..I` | Pattern selection when NOTE mode is off |
| `A/Z S/X D/C F/V` | Quick parameter controls |
| `T/G` | Oscillator +/- |
| `Y/H` | Filter type +/- |
| `N/M` | Distortion / Delay |
| `Ctrl+A/X/C/V` | Reset Cutoff / Resonance / Env Amount / Env Decay |

`Ctrl+A` selects every step of the pattern in PATTERN and DRUMS; `Alt+A` accents
the selected step. In SOUND, `Ctrl+A` resets the selected TB303 synth's cutoff. SOUND confirms the reset
with an `A FILTER 800Hz` or `B FILTER 800Hz` toast. Other synth engines show
`FILTER RESET: TB303 ONLY` and keep their parameters.

## DRUMS

Lane labels include their direct mute keys. Default mapping is `3KIK 4SNR 5HH1 6HH2 7PR1 8PR2 9RIM 0CLP`; SP12 follows its global swap as `0RIM 9CLP`. Accent is shown on the individual hit cell; there is no separate aggregate ACC row.

| Key | Action |
|---|---|
| `Tab` | Cycle DRUMS sub-pages: GRID / SOUND / AUTO (SOUND = the timbre; the former GLOBAL SETTINGS, now titled DRUM SOUND: drum engine and drum effects; SAMPLES exists only in the SDL emulator) |
| `Q..I` | Pattern 1..8 |
| `B` | Toggle bank A/B |
| `Alt+[` / `Alt+]` | Previous / next pattern page |
| `Arrows` | Move grid cursor |
| `Enter` | Toggle selected hit |
| `Backspace` | Erase the cell, or the selection |
| `Ctrl+Backspace` | Clear the WHOLE pattern of the slot as one Undo step and drop the selection (also when something is selected). Toasts `DRUMS CLEARED  CTRL+Z UNDO`, `DRUMS ALREADY CLEAR`, `DRUMS CLEAR FAILED` |
| `Alt+Backspace` | Does not touch the pattern; shows the hint `CTRL+BKSP CLEARS PATTERN` |
| `A` | Toggle accent on the selected existing hit only |
| `Alt+E` on the grid | DRUMS STEP SETTINGS window for the cursor cell (all selected cells when an area is selected; the first cell of the area gives the value, `MIXED` when the values in the area differ). `Up/Down` picks PROB (0..100, step 5), TIMING (-23..23 ticks, step 1), VEL (1..127, step 5) or FLAM/ROLL (OFF -> FLAM -> ROLL; FLAM is a second, softer hit 2 ticks later, ROLL is 4 rising hits in the step); `Left/Right` or `-`/`=` changes it; `Backspace` resets the selected field, `Ctrl+Backspace` all fields of the step (or of the selection); each key press is one Undo step (`Ctrl+Z` takes back only the last press). The window hint `L/R:SET  ENT/ESC:CLOSE` is unchanged. `Enter` or `Esc` closes; `Opt` and other keys are ignored while it is open. A RETRIG/REVERSE/PITCH step FX shows by name and is replaced by FLAM or ROLL. A field that differs from a plain step (PROB 100, TIMING 0, VEL 100, FLAM/ROLL OFF) shows a small square left of its name and an accent-coloured value; the default shows neither. Values move to the next multiple of 5, so 100 is always reachable. On the grid a cell with TIMING other than 0 shows a small square in its top corner (left = earlier, right = later), only on a step that sounds. A note cell with a 2x2 dot is a step whose PROB, VEL or FLAM/ROLL differs from the default |
| `Opt` alone, or `Alt+R`, on the grid | RHYTHM BROWSER: `Up/Down` picks a rhythm, `G` next group, `Alt+G` previous group (both wrap), `Left/Right` variant, `Tab` hear with the song (a change waits for the next bar), `M` solo, `1..0` mute while hearing (`1`/`2` SYN-A/SYN-B, `3..0` the rhythm's drum voices, same map as the lane labels; the browser does not draw mute state, the track screens do), `Enter` APPLY, `Esc` or `Opt` closes. The bottom hint is `UP/DN RHYTHM L/R VAR G/Alt+G GRP`; the info line shows `VAR n/m  N BPM`, then `TAB HEAR  G GROUP` before the audition and `1-0 MUTE  M SOLO` during it; the footer is `ENTER APPLY` / `ESC CLOSE`; an empty group shows `EMPTY`; there is no ATLAS/PRESET/VOCAB label. The position (rhythm, group, variant) is remembered between openings. A miniature at the right shows the selected rhythm as 16 steps x 8 drum voices (solid = hit, dim = soft hit); it is rebuilt only when the selection, variant or scene revision changes. Mutes are for listening only: they end with the audition (`Tab` again, `Enter`, `Esc`, leaving DRUMS, transport stop) and return to the mutes set before; APPLY writes all 8 voices, muted or not. APPLY writes the selected rhythm into the 8 drum voices of the current drum pattern (the slot shown as `ENTER: ALL 8 VOICES -> DRUMS A1`; toast `ALL 8 VOICES -> A1 CTRL+Z`); automation lanes and groove stay. One Undo step (`Ctrl+Z` on the grid restores the previous pattern). APPLY ends the audition, keeps the transport running and returns to the grid. Opens from the grid tab only |
| `G` | Generation on DRUMS: strong regeneration of RHYTHM + FEEL as one Undo step, at the current P-level |
| `Ctrl+G` | Randomize focused drum voice |
| `Alt+G` | Full-pattern CHAOS (in the RHYTHM BROWSER: previous group) |
| `P` | Cycle shared P1/P2/P3 request level (P1 faithful, P2 variation, P3 rework) |
| `Alt+O` | Cycle DRUMS output mode (toast `DRUMS OUT:`) |
| `Ctrl+C/V` | Copy / Paste |

## SONG

| Key | Action |
|---|---|
| `Left/Right` | Move `Synth A -> Synth B -> Drums`; crossing the outer edge changes edit Song slot A/B |
| `Up/Down` | Move Song row |
| `Enter` | Open the cell's pattern in its synth editor. On another page one Enter switches the page (`PAGE n  Bk`) and then opens the pattern automatically once the page has loaded. Refused while the working melody or steps are unsaved (`SAVE MELODY FIRST` / `SAVE STEPS FIRST`) |
| `Q..I` | Assign existing slot from visible `PAT:A/B` context |
| `G` | Generate safe material and assign selected cell; unique Song-generated material rerolls in place |
| double `G` | Materialize Synth A + Synth B + Drums for current row |
| `Alt+G` | Generate selected area |
| `Ctrl+G` | Cycle Song generator mode |
| `Backspace` | Clear current cell / selected Song cells |
| `Ctrl+Backspace` | Clear the whole Song (`Alt+Backspace` only shows the hint `CTRL+BKSP CLEARS SONG`) |
| `B` | Toggle visible `PAT:A/B` assignment bank |
| `Alt+B` | Flip stored-reference/selection bank |
| `Opt` | (Cardputer) Synth A/B column: that voice's source STEPS <-> MELODY, as `Opt` on the synth page |
| `Ctrl+B` | Play Song slot A/B from the next row. Pressed again on the same row before the boundary it cancels the switch (`PLAY SWITCH CANCELLED`); a reverse on the same row can share the boundary |
| `Alt+[` / `Alt+]` | Previous / next pattern page when the resident 16-slot page is full |
| `Ctrl+N` / `Ctrl+M` | Insert / remove row |
| `V` | Cycle lanes ALL -> AB -> DR |
| `X` | Split compare |
| `L` | Loop-lock around playhead |
| `Ctrl+L` | Loop mode |
| `Ctrl+R` | Reverse playback |
| `Alt+X` | LiveMix ON/OFF |
| `Ctrl+C/V` | Copy / Paste |
| `P` | Cursor to playhead |
| `Alt+J` | Jump to MATERIAL with this row as the explicit `TO` destination |
| `F` | FORM: energy curve. `Up/Down` energy 0..4, `Left/Right` section, `Enter` 4/8 bars, `N` add, `Backspace` delete, `1/2/3` BUILD/DROP/WAVE, `G` insert rows after the source block (selection, else the TAKE at the cursor), one Ctrl+Z; `F`/`Esc` back to GRID |

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
(`LENGTH 4B`, `STYLE P3`), then `D` while its source remains unedited and
unchanged. The result is `DEVELOP + BREAK 8B` or `BREAK ONLY 4B`; playback
changes at the next bar boundary. `Ctrl+Z` removes the added cycle in one step.
Non-4-bar, edited, P1/P2 Takes are refused (`P: SET P3, THEN G`); changing STYLE after making
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
| `R` | Cycle capture role |
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

### SONG FORM energy levels

`0` silence, `1` drums, `2` drums + bass (A), `3` + lead (B) resting on the
third bar of every four, `4` everything incl. the Voice lane. The curve builds
ordinary rows from existing material only; editing them later in GRID is never
undone by the curve.

### MIDI PLAYER LOOP

| Key | Action |
|---|---|
| `L` | Loop `OFF -> SONG -> A-B -> OFF` (A-B without marks: 4 bars from the playhead) |
| `S` / `A` | Mark A (loop start) at the current bar |
| `E` | Mark B (loop end, inclusive) at the current bar and loop A-B |

The header chip shows `LOOP` (whole song) or `L5-8` (bars). The loop restarts
through the ordinary start path, so each wrap has a short gap; in PROJECT tempo
it re-arms on the next master bar like a seek. A new file clears the A/B marks.

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
| `L` / `A` / `E` | The player loop, as on MIDI PLAYER: loop mode, mark A, mark B |
| `0` | All MIDI tracks on |
| `Space` | MIDI transport |
| `O` | SEQTRAK outputs: which tracks occupy KICK..CYM, SYN1, SYN2, DX, and which reach nothing (OFF) |
| `Y`, `Y` | GRAB (paused): the selected layer over the player's A-B loop (else GEN LENGTH bars from the current bar) becomes the Working Melody of GEN TARGET A/B; the first `Y` shows what and where, the second takes it; Ctrl+Z on the synth page undoes it |

Each row shows where the track sounds now: `SYN1`/`SYN2`/`DX`/`KICK`..`CYM`,
`DRUM` (GM drums split over the drum outputs), `MULTI`, or `OFF`. AUTO is
resolved from the source channel (CH1/2/3 -> SYN1/SYN2/DX, CH10 -> drums,
others -> OFF). A per-track choice is blue; OFF and two unmuted tracks on one
SYN1/SYN2/DX are red.

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
| `Enter` on Save | Save under the current name (unnamed project: asks for a name) |
| `A` on Save | Save As: type a new name |

MIDI import (PROJECT > Import MIDI, pick a file): the IMPORT track list shows one row
per channel (#, NAME, CH, N notes, RANGE, per-bar strip, TARGET).
`Up/Down` track, `T` target (A, B, DRUMS, SKIP), `Left/Right` bar cursor,
`S`/`E` range start/end at the bar cursor, `Enter` import, `Esc` back,
`Tab` old settings dialog (profile, start pattern, APPEND). Two tracks on A (or B)
refuse the import.

Load list: `Up` on the first item goes to the last and `Down` on the last goes
to the first. `Enter` loads. `R` renames the selected project (name editor,
`Enter` confirms, `Esc` cancels; an empty or already used name is refused).
`X` asks `DELETE <name>? ENTER=YES ESC=NO`; only `Enter` deletes, any other key cancels.
In the name editor `Left/Right` move the caret (an underline); typing and
`Backspace` act at the caret. There are no Home/End keys on this keyboard.

## TEMPO panel

Open with `Alt+Y` or `Fn+M -> TEMPO`. BPM is focused on open:
`Left/Right` changes it by 1, `Alt+Left/Right` by 5 (10–250 BPM).
Outside the panel `Ctrl+=` / `Ctrl+-` do the same by 1 (`Alt+=` / `Alt+-` by 5).

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
