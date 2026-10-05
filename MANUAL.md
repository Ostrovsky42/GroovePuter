# GroovePuter 0.9.14 Manual

This manual describes the user-facing workflows on the current 0.9.14 development
line. It focuses on Cardputer ADV controls and notes where the SDL keyboard differs.
For the exact key-by-key reference use [`src/ui/docs/keys.md`](src/ui/docs/keys.md).

For the current generated musical-play cycle, see
[`docs/0.9.14/P0_MUSICAL_PLAY_SPEC.md`](docs/0.9.14/P0_MUSICAL_PLAY_SPEC.md).

## 1. Workflow map

Main workflows:

```text
PERFORM:  MIDI KEYBOARD -> MIDI PLAYER
GENERATE: GENRE -> FEEL
HUB:      OVERVIEW -> SYNTH A -> SYNTH B -> DRUMS
SONG:     SONG -> MATERIAL -> MATERIAL BANK
SETTINGS: PROJECT / SETUP
```

Global navigation:

- `Fn+Tab` / `Fn+Shift+Tab`: next / previous workflow;
- `[` / `]`: previous / next page inside the workflow;
- `Fn+[` / `Fn+]`: previous / next workflow;
- `Fn+M`: workspace launcher;
- `Alt+H`: page-aware on-device help;
- `Alt+P`: MIDI Player;
- `Alt+V`: GENRE;
- `Alt+W`: waveform overlay except MATERIAL BANK `Alt+W` REPLACE;
- `Alt+X`: LiveMix;
- `Alt+M`: Song mode;
- `Alt+\`: public `CARBON <-> CYBER` theme cycle.

The active page receives input before global digit/mute fallbacks. `Alt+H` opens
page-aware help; use the on-screen page title and footer to confirm the current
context before using a page-specific shortcut.

### Compatibility page IDs

The following old persisted values are decode/navigation aliases only:

```text
GENERATION -> FEEL
TEXTURE    -> FEEL
Synth A SOUND -> SYNTH A
Synth B SOUND -> SYNTH B
```

There is no active standalone GENERATION, TEXTURE or SOUND workflow page.
Synth sound editing lives in each synth's local `NOTES -> KNOBS -> MORE` tabs.

## 2. GENRE and FEEL

The musical ownership rule is:

```text
GENRE != FEEL != GENERATION REQUEST != SOUND
```

### GENRE 1/2

GENRE owns the musical corridor, Variant/recipe, Rhythm identity and Apply policy.

Main controls:

- `G`: explicit full Stage 15 generation;
- `P`: `P1 CANON -> P2 VAR -> P3 TRANS`;
- `M`: apply policy (`PROFILE`, `MATERIALIZE`, `MATERIALIZE+BPM`);
- `Enter`: apply according to the selected policy;
- arrows/Tab: browse the visible Genre fields.

While stopped, accepted generation commits immediately. While PLAY is active, full
Genre material is prepared away from the sounding bar and the complete Synth A +
Synth B + Drums result publishes at the next real `BAR_START`. The transport is not
stopped/restarted around generation.

Repeated accepted `G` requests use a bounded session reroll attempt while keeping the
selected Genre/Variant/P-level composition identity.

### FEEL 2/2

FEEL owns timing and velocity only:

- timing profile;
- swing;
- bounded FEEL amount;
- velocity variation;
- repeat cycle `1/2/4/8`;
- FEEL presets;
- the same shared `P` selector.

FEEL does not select notes, harmony, synth TYPE or timbre.

### P1 / P2 / P3

```text
P1 CANON  clearest identity / least transformation
P2 VAR    recognizable variation; boot/session default
P3 TRANS  stronger related transformation where vocabulary allows
```

P3 is not CHAOS. Drums `Alt+G` is the separate explicit chaos command.

## 3. Synth A and Synth B

Current selectable synth engines are:

- `TB303`;
- `SID`;
- `AY`;
- `SH101`;
- `SN76489`;
- `WAVEMORPH`.

Legacy OPL2 scene values are decode-only; OPL2 is not a current selectable engine.

Each synth page owns one `Tab` cycle:

```text
[N]KM  NOTES
N[K]M  KNOBS
NK[M]  MORE
```

### NOTES

- `Q..I`: pattern slot `1..8` outside NOTE ENTRY;
- `B`: bank A/B;
- `Alt+[` / `Alt+]`: pattern page;
- arrows: step cursor;
- `N`: NOTE ENTRY on/off;
- `C` inside NOTE ENTRY: repeat the last entered pitch on the current step;
- `F`: toggle audible step retrigger; enabling starts at `R2`;
- `Alt+Up/Down`: adjust retrigger count `1..8` while retrigger is active;
- `G`: reroll only the selected Synth A or Synth B lane when NOTE ENTRY is off.

Selected-lane `G` uses the active Genre/Variant/Rhythm/P-level/harmony composition
identity. During PLAY it publishes at `BAR_START`. Drums and the neighboring synth are
not replaced. Inside NOTE ENTRY, `G` remains a note key.

### KNOBS / MORE

These tabs own synth TYPE, engine parameters and supported FX/sound editing. They are
not separate workflow pages.

## 4. Drums

The main drum-grid generation commands are deliberately distinct:

```text
G             drums-only strong generation at current P-level
Ctrl+G        randomize focused drum voice
Alt+G         explicit full-pattern CHAOS
Ctrl+Alt+G    Stage 12 phrase audition/probe
P             shared P1/P2/P3 selector
```

Pattern navigation remains `Q..I`, bank A/B and pattern page selection. The lane
labels include the matching global mute keys (`3KIK .. 0CLP`; SP12 uses `0RIM / 9CLP`). `A` toggles accent
only on the selected existing hit, and the accent marker is drawn on that hit rather
than in a separate aggregate ACC row.

## 5. Pattern, Melody and note entry

A pattern address is:

```text
PAGE 1..16 x BANK A/B x SLOT 1..8
```

Example: `2B7`.

Synth A and Synth B slots can contain either Pattern steps or an accepted Melody.
On a Synth page, `Alt+R` switches between `STEPS` and `MELODY`. If a slot has no
Melody yet, switching to MELODY first creates one from its Pattern steps. On the
MELODY source, `Q..I` selects the accepted Melody in that slot and `B` selects the
same slot number in the other bank. An empty slot reports `NO MELODY`. If the current
working Melody has unsaved edits, changing slots asks for `Alt+Enter SAVE` instead of
silently replacing those edits.

In `STEPS`, press `N` to enable NOTE ENTRY. Note keys enter pitches at the selected
step. Repeating or holding the same pitch can continue it into the next step as a
tie; the continuation is not a new note attack. The grid displays a continuation as
`TI`. `Z` is a pitch key only while NOTE ENTRY is active. Check the selected source
and step shown on screen before editing.

Pattern address remains `PAGE × BANK × SLOT`, for example `2B7`. Project-scoped
storage keeps one project's pattern pages separate from another's. Song/Phrase
generation checks its destination rather than silently overwriting referenced
material.

## 6. Song

Song has two arrangement slots, A and B. Each row stores a slot number for a track;
for Synth A and Synth B, that slot's saved descriptor determines whether playback
uses Pattern steps or Melody. The row does not store a second copy of the material
type. A saved arrangement can therefore play Pattern X → Melody Y → Pattern Z
without manual source switching, including after project reload, when the Melody data
has been accepted and saved.

During playback the row display identifies a synth as `PAT`, `MEL`, `HOLD`, `WAIT`
or `FAIL`. A working Melody can keep sounding while it is edited. Use `Alt+Enter`
to accept the edited Melody or `Alt+X` to discard the edits and return to the saved
material assigned to the current Song row. Song prepares an upcoming Melody before
its row boundary. Manual NEXT is refused only when it would conflict with the next
Song Melody for that synth.

Horizontal edit navigation is one bounded strip across Synth A → Synth B → Drums;
crossing the outer track edge moves between edit Song A/B.

Important bank/slot controls:

- `B`: change visible `PAT:A/B` assignment context only;
- `Alt+B`: flip stored-reference/selection bank;
- `Ctrl+B`: choose playback Song slot A/B;
- `Q..I`: assign an existing slot from the visible pattern context;
- `G`: generate safe free material and assign the selected cell;
- double `G`: materialize Synth A + Synth B + Drums for the current row as one logical mutation;
- `Ctrl+N` / `Ctrl+M`: insert / delete row.

Copy-on-write generation must not silently replace a pattern still referenced by other
Song/Phrase locations.

Song references a slot; playback resolves the slot's current saved material kind.
Save after editing/accepting a Melody and assigning the arrangement so its Melody
data, descriptor and Song rows are available together after reload.

## 7. MATERIAL and Phrase workflows

### MATERIAL — generated Phrase

The `MATERIAL` page creates generated phrases in the Song arrangement. Its request
fields are `LENGTH`, `STYLE` and `TO`; the last accepted phrase and its Song rows are
shown separately. Up/Down selects a field and Left/Right changes it. `G` creates a
TAKE at the displayed destination. `TO APPEND` follows the current end of Song;
`TO EXPLICIT` addresses the selected row. The page displays `FREE`, `OCCUPIED`,
`NO ROOM` or `NO SLOTS` before generation.

To create the current short development cycle:

1. Set `LENGTH 4B` and `STYLE REWORK` (P3).
2. Press `G` to create a new TAKE.
3. While that generated TAKE remains unedited and in the same source context, press
   `D` (`GROW`).
4. The page reports either `DEVELOP + BREAK 8B` or `BREAK ONLY 4B` and shows the
   Song rows. During playback, the new rows activate at the next bar boundary.
5. Press `Ctrl+Z` to undo the complete added cycle in one step.

This operation requires a fresh, unedited 4-bar TAKE made at P3. The default STYLE is
P2, so choose REWORK before pressing `G`; changing STYLE afterwards does not change
the existing TAKE. Some styles are not admitted. Editing the TAKE, changing its
source context, or lacking Song rows/pattern slots causes a clear refusal instead of
growth. Acid and House, edited-source growth, repeated multi-cycle development and
persistent development history are outside this first slice. The separate `D`
command on `MATERIAL BANK` means derive and is not `GROW`.

When `G` or `D` answers `NO ROOM: R MAKES ROOM`, the page has no run of consecutive free pattern slots,
usually because earlier generated patterns are still stored after their Song rows were deleted. Press plain `R`
on `MATERIAL`. The page asks `REUSE n UNUSED TAKES?`: takes generated in this session, not in Song and not edited
by you. `Enter` says yes (you return with `ROOM FOR 4B: PRESS G`), `Esc` says no. Nothing is erased: the next `G`/`D`
uses free slots first and replaces these only when nothing else fits. **After a replacement Undo restores the Song
rows but not the old content.** Older material, hand-made or edited patterns, anything in Song, the CURRENT selection
and the live Undo are never offered automatically; press `S` on the question to choose slots yourself in a grid
(`~` unused, `*` allowed, letters mark what holds a slot). Permissions are session-only: they end on scene load,
new scene and page change, and editing a slot cancels its permission.

Typical deletion path before using it: on `SONG`, `Backspace` clears the selected cell(s), or `Ctrl+M` removes the
row. A row deleted this way no longer references its pattern, but the pattern stays in its slot until replaced.

### MATERIAL BANK — Phrase Core

`MATERIAL BANK` is the legacy capture/derive/write workspace. It has four saved
slots (`A/B/C/D`) and a Song destination `TO:`.

Main controls:

```text
1..4              select Phrase A/B/C/D
Up/Down           length 1/2/4/8 bars
Left/Right        preview saved Phrase bar
Ctrl+Left/Right   TO +/-1 row
Ctrl+Up/Down      TO +/-8 rows
Enter             capture current Song region
D                 derive parent into selected slot
G                 generate fresh connected material at TO
W                 INSERT saved Phrase before TO and shift later rows
Alt+W             REPLACE Phrase lanes at TO without row shift
```

Fresh multi-row Phrase generation is deliberately STOP-only. During PLAY it reports
`STOP PLAYBACK FOR PHRASE` instead of stopping and restarting transport implicitly.
Successful `G` or `W` advances `TO:` by the Phrase length. These controls and their
destination are independent of the generated-Phrase request on `MATERIAL`.

Phrase storage remains `REFERENCE VIEW / REF MUTABLE`: saved Phrase slots keep bounded
references to pattern material rather than secretly taking a second copy of note
ownership.

## 8. PERFORM and PERFORMANCE TOOLS

MIDI KEYBOARD provides the live scale-aware QWERTY performance surface. `Tab` opens
PERFORMANCE TOOLS. On Cardputer ADV the physical comma/period positions are
navigation arrows, so change SCALE from the KEY tools row with `Left/Right`;
the `, / .` shortcuts remain SDL/external-keyboard compatibility only:

| Key | Tool |
|---|---|
| `1` | ARPEGGIATOR |
| `2` | DIRECTION |
| `3` | CHORD |
| `4` | MEMORY |
| `5` | STRUM |
| `6` | RATCHET |
| `7` | EUCLIDEAN |
| `8` | ROTATE |
| `9` | receiver `MONO/POLY` |
| `-` / `+` | performance velocity `10..120` |

Receiver MONO/POLY is an external-MIDI receiver contract. Internal Synth A/B remain
sequencer/pattern instruments. SEQTRAK Synth/DX targets use the current receiver-mode
MIDI control path.

## 9. MIDI Player and HUB MIDI

Open MIDI Player with `Alt+P`, choose a file and press `Enter`.

Main Player controls include:

- `Space`: MIDI transport;
- `1..9`: physical-track mute;
- `U`: physical mute mixer;
- `I`: channel inspector;
- `S`: structural inspector;
- `D`: performance/throughput panel;
- `H`: Player <-> HUB MIDI;
- `M`: RAW / SEQTRAK routing mode;
- `C`: clock source;
- `T`: tempo mode;
- `R`: restart file;
- `X`: SMF-owned note cleanup/panic.

### HUB MIDI

With a loaded MIDI session:

- `Up/Down`: select projected physical layer;
- plain `Left/Right`: change route immediately (`AUTO`, `CH1..CH10`), including during PLAY;
- `Fn+Left/Right`: physical-track level;
- `Enter` or `1..9`: mute/unmute;
- `S`: solo;
- `A`: all MIDI tracks on;
- `H`: return to Player.

There is no pause-first or Enter-to-commit route-edit mode. Route revisions reject
stale queued events from the previous target and scoped cleanup NoteOff prevents stuck
notes without a global panic.

RAW routing preserves source channels and does not accept explicit SEQTRAK destination
overrides. SEQTRAK-safe mapping uses drums on `CH1..CH7`, Synth 1 on `CH8`, Synth 2 on
`CH9`, and DX on `CH10`.

## 10. Project save, ACCEPT, DISCARD and Undo

Project Save/Load is reached from the Project page. Use its on-screen scene
selection and actions; this is separate from `Alt+Enter`, which accepts working
material on the Synth pages.

On Synth A/B material:

- `D` prepares a REVOICE candidate in NEXT; `Alt+V` prepares CONNECT. These actions
  prepare material and do not immediately replace the sounding CURRENT.
- `Enter` requests GO. While playing, NEXT activates at a musical bar boundary.
  `Esc` cancels a pending candidate, or disarms a queued GO while keeping NEXT.
- `Alt+Enter` ACCEPTs working material as the accepted version.
- `Alt+Backspace` or `Alt+X` DISCARDs working edits and restores the accepted version.
- `Ctrl+Z` undoes the last retained edit on a supported page. Undo is one-step, not
  a durable project history.

Pattern/Melody data, Song references, synth TYPE and supported parameters,
project-scoped pattern pages, Phrase state and supported UI state are persisted
through their respective project storage paths. Save after changing arrangement or
scene state. Accepted Melody data is stored in its selected slot; save the project
so its Song descriptor and arrangement return together after reload.

A loaded synth patch remains the owner of its saved TYPE/parameters; loading a project
must not silently replace it with hidden genre timbre defaults.

Historical release checklists apply to the specific release named in each document;
they are not a current feature list.

## 11. Waveform HUD

The bottom performance HUD has one compositing owner. The optional waveform is cleared
and redrawn without accumulating stale pixels, runs beneath mute/activity digits, and
uses bounded visual auto-gain. MIDI Player uses the taller progress waveform from the
current MIDI Player HUD.

## 12. Developer build and flash

```bash
bash scripts/install_arduino_deps.sh
bash tests/run_host_tests.sh
bash scripts/build_cardputer_dynbuffers.sh
bash scripts/check_cardputer_dram_budget.sh \
  build/cardputer-adv-dynbuffers/GroovePuter.ino.elf
bash scripts/build_seqtrak_midi_only.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

## 13. Feature boundaries

Keep these distinctions in mind when interpreting visible controls:

- The four-slot arrangement Phrase Bank is reachable. The separate eight-slot
  QWERTYUI state helper is not wired into the production workflow.
- REVOICE and CONNECT prepare NEXT candidates. They do not directly replace accepted
  material; use GO to activate a candidate.
- The current DEVELOP workflow is the bounded MATERIAL cycle above. Arbitrary
  multi-bar growth from an edited Pattern or Melody is not available.
- Existing USB MIDI Device, DIN and SEQTRAK paths are separate from USB Host. USB
  Host/nanoKEY2 is not part of the supported workflow described here.
- Song synth rows resolve Pattern or Melody slots. Other kinds of material are not
  supported as Song-row sources, and development history is not retained across
  project reload.

For release acceptance status, use current documents under [`docs/releases/`](docs/releases/)
and [`docs/0.9.14/`](docs/0.9.14/).
