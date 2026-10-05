# GroovePuter 0.9.17 Manual

This manual describes the current user-facing GroovePuter workflow on the 0.9.17 productization line, built on the hardware-accepted **v0.9.16 Foundation Freeze**.

If this is your first session, do **not** start by reading the whole manual. Use [`docs/user/QUICKSTART.md`](docs/user/QUICKSTART.md) first. It reduces the first-session vocabulary to `Space`, `Alt+V`, `G`, `D`, `Ctrl+Z` and `Alt+H`.

For the exact key-by-key reference use [`src/ui/docs/keys.md`](src/ui/docs/keys.md). For the bounded generated musical-play implementation and its research evidence see [`docs/0.9.14/P0_MUSICAL_PLAY_SPEC.md`](docs/0.9.14/P0_MUSICAL_PLAY_SPEC.md).

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

The active page receives input before global fallbacks. Use the page title and footer to confirm context before using a page-specific shortcut.

### Compatibility page IDs

The following persisted values are decode/navigation aliases only:

```text
GENERATION -> FEEL
TEXTURE    -> FEEL
Synth A SOUND -> SYNTH A
Synth B SOUND -> SYNTH B
```

There is no active standalone GENERATION, TEXTURE or SOUND workflow page. Synth sound editing lives in each synth's local `NOTES -> KNOBS -> MORE` tabs.

## 2. First-session key constitution

The beginner vocabulary is deliberately smaller than the full key map:

```text
Space     PLAY / STOP
Alt+V     GENRE
G         generate the thing on screen
D         DEVELOP on MATERIAL
Ctrl+Z    undo the last retained edit
Alt+H     help for this page
```

`G` is intentionally contextual. On GENRE it generates full musical material; on Synth A/B it generates that lane; on DRUMS it generates drums; on SONG or MATERIAL it acts on the visible material workflow.

`D` is **not** one global verb. In the First Five Minutes path it means DEVELOP only on MATERIAL. MATERIAL BANK keeps `D` as derive, and Synth pages keep their own development/edit semantics.

## 3. GENRE and FEEL

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

While stopped, accepted generation commits immediately. While PLAY is active, full Genre material is prepared away from the sounding bar and the complete Synth A + Synth B + Drums result publishes at the next real `BAR_START`. The transport is not stopped/restarted around generation.

Repeated accepted `G` requests use a bounded session reroll attempt while keeping the selected Genre/Variant/P-level composition identity.

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

The expert/internal request levels remain:

```text
P1 CANON  clearest identity / least transformation
P2 VAR    recognizable variation; boot/session default
P3 TRANS  stronger related transformation where vocabulary allows
```

On user-facing Synth generation the same intent is presented as `FAITHFUL -> VARIANT -> REWORK`. New users do not need to learn the P-level terminology before making music.

P3 is not CHAOS. Drums `Alt+G` is the separate explicit chaos command.

## 4. Synth A and Synth B

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
- `G`: reroll only the selected Synth A or Synth B lane when NOTE ENTRY is off;
- `P`: choose the shared `FAITHFUL / VARIANT / REWORK` generation intent while on STEPS;
- `Alt+G`: legacy selected-synth generator, without the shared STYLE request.

Selected-lane `G` uses the active Genre/Variant/Rhythm/P-level/harmony composition identity. During PLAY it publishes at `BAR_START`. Drums and the neighboring synth are not replaced. Inside NOTE ENTRY, `G` remains a note key.

### Pattern and Melody source

A pattern address is:

```text
PAGE 1..16 x BANK A/B x SLOT 1..8
```

Example: `2B7`.

Synth A and Synth B slots can contain either Pattern steps or an accepted Melody. On a Synth page, `Alt+R` switches between `STEPS` and `MELODY`. If a slot has no Melody yet, switching to MELODY first creates one from its Pattern steps. On the MELODY source, `Q..I` selects the accepted Melody in that slot and `B` selects the same slot number in the other bank. An empty slot reports `NO MELODY`. If the current working Melody has unsaved edits, changing slots asks for `Alt+Enter SAVE` instead of silently replacing those edits.

In STEPS, press `N` to enable NOTE ENTRY. Repeating or holding the same pitch can continue it into the next step as a tie; the continuation is not a new note attack. The grid displays a continuation as `TI`.

On the Melody editor, `Left/Right` move along time and `Up/Down` change pitch in piano-roll view (`V` switches to list view). `Enter` adds a note, `Backspace` deletes the note at the cursor, `Alt+Left/Right` changes duration, `J` joins to the next note, `G` changes the grid, `[` / `]` move by bar, and `L` / `Alt+L` change Melody length when safe.

### ACCEPT / DISCARD / NEXT

On Synth A/B material:

- `D` prepares a REVOICE candidate in NEXT; `Alt+V` prepares CONNECT where that local page owns the shortcut;
- `Enter` requests GO. While playing, NEXT activates at a musical bar boundary;
- `Esc` cancels NEXT, or disarms a queued GO while keeping NEXT;
- `Alt+Enter` ACCEPTs working material as the accepted version;
- `Alt+Backspace` or `Alt+X` DISCARDs working edits and restores the accepted version;
- `Ctrl+Z` undoes the last retained edit on a supported page.

These expert Synth meanings are why the 0.9.17 beginner rule says `D = DEVELOP` only on MATERIAL rather than pretending `D` is globally identical everywhere.

### KNOBS / MORE

These tabs own synth TYPE, engine parameters and supported FX/sound editing. They are not separate workflow pages.

Useful quick controls include `A/Z S/X D/C F/V`, oscillator `T/G`, filter type `Y/H`, Distortion/Delay `N/M`, and `Ctrl+A/X/C/V` parameter resets. See the key map for the exact current mapping.

## 5. Drums

The main drum-grid generation commands are deliberately distinct:

```text
G             drums-only strong generation at current P-level
Ctrl+G        randomize focused drum voice
Alt+G         explicit full-pattern CHAOS
Ctrl+Alt+G    Stage 12 phrase audition/probe
P             shared P1/P2/P3 selector
```

Pattern navigation remains `Q..I`, bank A/B and pattern page selection. The lane labels include the matching global mute keys (`3KIK .. 0CLP`; SP12 uses `0RIM / 9CLP`). `A` toggles accent only on the selected existing hit, and the accent marker is drawn on that hit rather than in a separate aggregate ACC row.

## 6. Song

Song has two arrangement slots, A and B. Each row stores a slot number for a track; for Synth A and Synth B, that slot's saved descriptor determines whether playback uses Pattern steps or Melody. The row does not store a second copy of the material type. A saved arrangement can therefore play Pattern X -> Melody Y -> Pattern Z without manual source switching, including after project reload, when the Melody data has been accepted and saved.

During playback the row display identifies a synth as `PAT`, `MEL`, `HOLD`, `WAIT` or `FAIL`. A working Melody can keep sounding while it is edited. Use `Alt+Enter` to accept the edited Melody or `Alt+X` to discard the edits and return to the saved material assigned to the current Song row.

Horizontal edit navigation is one bounded strip across Synth A -> Synth B -> Drums; crossing the outer track edge moves between edit Song A/B.

Important bank/slot controls:

- `B`: change visible `PAT:A/B` assignment context only;
- `Alt+B`: flip stored-reference/selection bank;
- `Ctrl+B`: choose playback Song slot A/B;
- `Q..I`: assign an existing slot from the visible pattern context;
- `G`: generate safe free material and assign the selected cell;
- double `G`: materialize Synth A + Synth B + Drums for the current row as one logical mutation;
- `Ctrl+N` / `Ctrl+M`: insert / delete row.

Copy-on-write generation must not silently replace a pattern still referenced by other Song/Phrase locations.

## 7. MATERIAL — generated musical play

MATERIAL is the first-session composition page. Its request fields are `LENGTH`, `STYLE` and `TO`; the last accepted phrase and its Song rows are shown separately.

The beginner mental model is:

```text
G  -> NEW TAKE
D  -> DEVELOP that eligible fresh TAKE
R  -> MAKE ROOM if slots are exhausted
```

For the current bounded development cycle:

1. Set `LENGTH 4B`.
2. Set `STYLE REWORK`.
3. Leave `TO APPEND` for the simplest flow.
4. Press `G` to create a fresh TAKE.
5. While that generated TAKE remains unedited and in the same source context, press `D`.
6. The page reports either `DEVELOP + BREAK 8B` or `BREAK ONLY 4B` and shows the Song rows.
7. Press `Ctrl+Z` to undo the complete added cycle in one step if you do not want it.

The engine still has a narrower internal admissibility contract than this musical wording. The UI/manual should explain refusals in terms of a fresh unedited `4B REWORK` take rather than requiring a new user to understand semantic/depth internals.

Acid and House, edited-source growth, repeated multi-cycle development and persistent development history remain outside this bounded slice unless a later release explicitly says otherwise.

### Make room

When `G` or `D` reports `NO ROOM`, press plain `R` on MATERIAL. The page asks whether it may reuse unused generated takes from the current session that are not in Song and have not been edited. `Enter` says yes; `Esc` cancels; `S` opens the slot-by-slot expert view.

Nothing is erased merely by granting permission. The next generation uses truly free slots first and replaces allowed slots only when needed. Older material, hand-made or edited patterns, anything in Song, CURRENT, working edits, queued NEXT and the live Undo are protected by the existing ownership rules.

## 8. MATERIAL BANK — Phrase Core

MATERIAL BANK is the legacy capture/derive/write workspace. It has four saved slots (`A/B/C/D`) and a Song destination `TO:`.

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

Fresh multi-row Phrase generation is deliberately STOP-only. During PLAY it reports `STOP PLAYBACK FOR PHRASE` instead of stopping and restarting transport implicitly.

The separate `D` command on MATERIAL BANK means derive and is not GROW/DEVELOP.

## 9. PERFORM and external keyboard

MIDI KEYBOARD provides the live scale-aware QWERTY performance surface. `Tab` opens PERFORMANCE TOOLS. The KEY / CHORD / ARP / RHYTHM contexts expose scale, output target, receiver mode and performance transformations without adding another note owner.

The v0.9.16 Foundation Freeze includes the accepted external USB Host keyboard path through PERFORM. The PROJECT MIDI setup selects the boot USB role (`COMPUTER`, `KEYBOARD`, or `OFF`) and the role is applied across the explicit reboot boundary. A connected external keyboard feeds the same PERFORM musical behavior rather than bypassing it with a second performance engine.

PERFORM supports the accepted CHORD / ARP / LATCH / rhythm / mono-poly paths documented in the release evidence. Receiver MONO/POLY is an external-MIDI receiver contract; internal Synth A/B remain sequencer/pattern instruments.

## 10. MIDI Player and HUB MIDI

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

There is no pause-first or Enter-to-commit route-edit mode. Route revisions reject stale queued events from the previous target and scoped cleanup NoteOff prevents stuck notes without a global panic.

RAW routing preserves source channels and does not accept explicit SEQTRAK destination overrides. SEQTRAK-safe mapping uses drums on `CH1..CH7`, Synth 1 on `CH8`, Synth 2 on `CH9`, and DX on `CH10`.

## 11. Project save and Undo

Project Save/Load is reached from the Project page. Use its on-screen scene selection and actions; this is separate from `Alt+Enter`, which accepts working material on the Synth pages.

`Ctrl+Z` is the public global Undo chord for retained edits supported by the active context. Undo is intentionally bounded one-step ownership, not a durable project history.

Pattern/Melody data, Song references, synth TYPE and supported parameters, project-scoped pattern pages, Phrase state and supported UI state are persisted through their respective project storage paths. Save after changing arrangement or scene state. Accepted Melody data is stored in its selected slot; save the project so its Song descriptor and arrangement return together after reload.

A loaded synth patch remains the owner of its saved TYPE/parameters; loading a project must not silently replace it with hidden genre timbre defaults.

## 12. Help strategy

`Alt+H` is the primary manual while holding the device. It is page-aware and should answer "what can I do here?" before exposing implementation terminology.

Use documentation in this order during normal play:

1. on-screen footer;
2. `Alt+H` page help;
3. [`docs/user/QUICKSTART.md`](docs/user/QUICKSTART.md) for the first-session workflow;
4. [`src/ui/docs/keys.md`](src/ui/docs/keys.md) for the complete key map;
5. this manual for workflow details;
6. research/architecture documents only when investigating implementation or design history.

## 13. Developer build and flash

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

For a user installation, prefer a release/Launcher path over requiring the user to build with Arduino tooling. Exact release artifacts and accepted hashes belong to the corresponding GitHub release.

## 14. Product boundaries

- REVOICE and CONNECT prepare NEXT candidates. They do not directly replace accepted material; use GO to activate a candidate.
- The current DEVELOP workflow is the bounded MATERIAL cycle above. Arbitrary multi-bar growth from an edited Pattern or Melody is not available yet.
- Song synth rows resolve Pattern or Melody slots. Other kinds of material are not silently treated as Song-row sources.
- Development history is not retained as an unbounded lineage graph across project reload.
- 0.9.17 is a usability/productization line. It should not reopen the 0.9.16 Foundation Freeze for speculative architecture.

For release acceptance status, use the exact GitHub release and current documents under [`docs/releases/`](docs/releases/). Historical release checklists apply only to the release named in each document.
