# GroovePuter 0.9.1 Manual

This manual describes the user-visible Cardputer ADV runtime shipped by the 0.9.1
release line. Persisted compatibility IDs and old stage documents are not proof that a
page is still reachable.

For exact key-by-key behavior use [`src/ui/docs/keys.md`](src/ui/docs/keys.md).
For the release freeze and acceptance boundary use
[`docs/releases/0_9_1_RELEASE.md`](docs/releases/0_9_1_RELEASE.md).

## 1. Workflow map

GroovePuter 0.9.1 has **11 active pages** in five workflows:

```text
PERFORM:  MIDI KEYBOARD -> MIDI PLAYER
GENERATE: GENRE -> FEEL
HUB:      OVERVIEW -> SYNTH A -> SYNTH B -> DRUMS
SONG:     SONG -> PHRASE CORE
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
- `Alt+W`: waveform overlay except Phrase `Alt+W` REPLACE;
- `Alt+X`: LiveMix;
- `Alt+M`: Song mode;
- `Alt+\`: public `CARBON <-> CYBER` theme cycle.

The active page receives input before global digit/mute fallbacks.

### Compatibility page IDs

The following old persisted values are decode/navigation aliases only:

```text
GENERATION -> FEEL
TEXTURE    -> FEEL
Synth A SOUND -> SYNTH A
Synth B SOUND -> SYNTH B
```

There is no active standalone GENERATION, TEXTURE or SOUND workflow page in 0.9.1.
Synth sound editing lives in each synth's local `NOTES -> KNOBS -> MORE` tabs.

## 2. GENRE and FEEL

The release ownership rule is:

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

Pattern navigation remains `Q..I`, bank A/B and pattern page selection. Editing owns
its own cursor, accent and selection commands.

## 5. Pattern identity and project storage

A pattern address is:

```text
PAGE 1..16 x BANK A/B x SLOT 1..8
```

Example: `2B7`.

Project-scoped pattern storage prevents one project from silently reusing another
project's pattern-page namespace. Song/Phrase generation uses safe destination checks
rather than overwriting referenced material without validation.

## 6. Song

Song has two arrangement slots, A and B. Horizontal edit navigation is one bounded
strip across Synth A -> Synth B -> Drums; crossing the outer track edge moves between
edit Song A/B.

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

## 7. Phrase Core

Phrase Core is the second SONG page. It has four saved slots (`A/B/C/D`) and one
visible Song destination, `TO:`.

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
Successful `G` or `W` advances `TO:` by the Phrase length.

Phrase storage remains `REFERENCE VIEW / REF MUTABLE`: saved Phrase slots keep bounded
references to pattern material rather than secretly taking a second copy of note
ownership.

## 8. PERFORM and PERFORMANCE TOOLS

MIDI KEYBOARD provides the live scale-aware QWERTY performance surface. `Tab` opens
PERFORMANCE TOOLS:

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

## 10. Persistence and recovery

0.9.1 release acceptance includes:

- Scene Save/Load;
- project-scoped pattern storage;
- independent Synth A/B TYPE and visible parameter persistence;
- safe legacy decode/defaults;
- Song references;
- Phrase Core state;
- supported UI-session state.

A loaded synth patch remains the owner of its saved TYPE/parameters; loading a project
must not silently replace it with hidden genre timbre defaults.

The older `PRE_0_9_RELEASE_GATE.md` and `0_9_FINAL_ACCEPTANCE.md` documents are retained
as historical 0.9 evidence. They are **not** the current 0.9.1 release gate.

## 11. Waveform HUD

The bottom performance HUD has one compositing owner. The optional waveform is cleared
and redrawn without accumulating stale pixels, runs beneath mute/activity digits, and
uses bounded visual auto-gain. MIDI Player uses the taller progress waveform from the
final 0.9.1 HUD fix.

## 12. Build and flash

```bash
bash scripts/install_arduino_deps.sh
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh \
  build/cardputer-adv-current/GroovePuter.ino.elf
bash scripts/build_seqtrak_midi_only.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

## 13. Release acceptance

The normative checklist and exact frozen runtime SHA are in
[`docs/releases/0_9_1_RELEASE.md`](docs/releases/0_9_1_RELEASE.md).

A new feature, architecture cleanup or research admission after that boundary belongs
to the next release line unless it fixes a concrete 0.9.1 correctness defect.
