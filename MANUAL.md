# GroovePuter 0.9.17 Manual

This manual describes the current user-facing GroovePuter workflow on the 0.9.17 productization line, built on the hardware-accepted **v0.9.16 Foundation Freeze**.

If this is your first session, do **not** start by reading the whole manual. Use [`docs/user/QUICKSTART.md`](docs/user/QUICKSTART.md) first. Its first-session vocabulary is deliberately small: `Space`, `Fn+M`, `G`, `D`, `Ctrl+Z` and `Alt+H`.

For the exact key-by-key reference use [`src/ui/docs/keys.md`](src/ui/docs/keys.md). For the bounded generated musical-play implementation and its research evidence see [`docs/0.9.14/P0_MUSICAL_PLAY_SPEC.md`](docs/0.9.14/P0_MUSICAL_PLAY_SPEC.md).

## 1. Workflow map

```text
PERFORM:  MIDI KEYBOARD -> MIDI PLAYER
GENERATE: GENRE -> FEEL
HUB:      OVERVIEW -> SYNTH A -> SYNTH B -> DRUMS
SONG:     SONG -> MATERIAL -> MATERIAL BANK
SETTINGS: PROJECT / SETUP
```

The main navigation model is:

- `Fn+M`: workspace launcher;
- `Fn+Tab` / `Fn+Shift+Tab`: next / previous workflow;
- `[` / `]`: previous / next page inside the current workflow;
- `Fn+[` / `Fn+]`: previous / next workflow;
- `Alt+H`: page-aware on-device help;
- `Alt+P`: MIDI Player;
- `Alt+X`: LiveMix;
- `Alt+M`: Song mode;
- `Alt+\`: `CARBON <-> CYBER` theme cycle.

### A current navigation defect worth knowing

The v0.9.16 foundation still routes hard-global `Alt+V` through legacy page id 11. That id normalizes to **FEEL**, not GENRE. Therefore the truthful 0.9.17 first-session path is:

```text
Fn+M -> GENRE
```

not `Alt+V -> GENRE`.

This is a bounded 0.9.17 navigation defect. It does not change Genre generation itself.

Hard-global Alt shortcuts run before page-local input. That is why a page must not document a local action on a chord already owned globally.

### Compatibility page IDs

The following persisted values remain decode/navigation aliases:

```text
GENERATION -> FEEL
TEXTURE    -> FEEL
Synth A SOUND -> SYNTH A
Synth B SOUND -> SYNTH B
```

There is no active standalone GENERATION, TEXTURE or SOUND workflow page. Synth sound editing lives in each Synth page's local `NOTES -> KNOBS -> MORE` tabs.

## 2. First-session key constitution

```text
Space     PLAY / STOP
Fn+M      choose GENRE / MATERIAL / other workspace
G         generate the thing on screen
D         DEVELOP on MATERIAL
Ctrl+Z    undo the last retained edit
Alt+H     help for this page
```

These are musician-facing rules, not a claim that every key is globally identical.

### `G` — Generate the thing you are looking at

- GENRE: generate the full musical material;
- Synth A/B: generate that selected Synth lane when NOTE ENTRY is off;
- DRUMS: generate drums;
- SONG: generate/assign material in the selected Song context;
- MATERIAL: create a TAKE at the displayed destination.

### `D` — contextual, not global

In the First Five Minutes workflow, `D` means **DEVELOP** only on MATERIAL.

Elsewhere it has page-local expert meanings. MATERIAL BANK, for example, uses `D` for derive. Do not infer a global `D = develop` rule from the beginner path.

## 3. GENRE and FEEL

The ownership rule is:

```text
GENRE != FEEL != GENERATION REQUEST != SOUND
```

### GENRE 1/2

GENRE owns the musical corridor, Variant/recipe, Rhythm identity and Apply policy.

Main controls:

- `Tab` / `Up/Down`: select the visible field;
- `Left/Right`: change the selected Genre/Variant/Rhythm/Apply field;
- `G`: full Stage 15 generation;
- `P`: `P1 CANON -> P2 VAR -> P3 TRANS`;
- `M`: cycle `PROFILE`, `MATERIALIZE`, `MATERIALIZE+BPM`;
- `Enter`: apply the selected policy.

While stopped, accepted generation commits immediately. During PLAY the full Synth A + Synth B + Drums result publishes at the next real `BAR_START` instead of replacing the sounding bar mid-cycle.

Repeated accepted `G` requests reroll the same selected musical direction through the bounded session attempt stream.

### FEEL 2/2

FEEL owns timing and velocity only:

- timing profile;
- swing;
- bounded FEEL amount;
- velocity variation;
- repeat cycle `1/2/4/8`;
- FEEL presets;
- the shared request level.

FEEL does not select notes, harmony, synth TYPE or timbre.

### FAITHFUL / VARIANT / REWORK versus P1 / P2 / P3

The engine still uses the bounded P-level vocabulary internally:

```text
P1 CANON
P2 VAR
P3 TRANS
```

On the user-facing Synth path the same intent is presented more musically as:

```text
FAITHFUL -> VARIANT -> REWORK
```

A new user should not need to understand the internal P naming before making music.

## 4. SYNTH A / SYNTH B

Current selectable engines include:

- `TB303`;
- `SID`;
- `AY`;
- `SH101`;
- `SN76489`;
- `WAVEMORPH`.

Each Synth page uses:

```text
NOTES -> KNOBS -> MORE
```

with `Tab` cycling the local tabs.

### NOTES

Important controls:

- `Q..I`: pattern slot `1..8` outside NOTE ENTRY;
- `B`: bank A/B;
- `Alt+[` / `Alt+]`: pattern page;
- arrows: step cursor;
- `N`: NOTE ENTRY on/off;
- `C`: repeat the last entered pitch at the current step while in NOTE ENTRY;
- `F`: audible step retrigger on/off;
- `Alt+Up/Down`: retrigger count;
- `G`: selected-Synth Genre/recipe/STYLE/harmony generation when NOTE ENTRY is off;
- `P`: `FAITHFUL / VARIANT / REWORK` for the next selected-Synth `G`;
- `Alt+G`: legacy selected-Synth generation without the shared STYLE request;
- `Ctrl+C/V`: Copy / Paste.

Inside NOTE ENTRY, pitch keys retain their note-input meaning. A repeated/held pitch may continue into the next step as a `TI` continuation rather than a new attack.

### Pattern and Melody source

A Pattern address is:

```text
PAGE x BANK x SLOT
```

for example `2B7`.

`Alt+R` switches a Synth slot between STEPS and MELODY source. If no accepted Melody exists yet, the conversion path creates one from the Pattern steps first. A Song row later resolves the slot's saved material descriptor; it does not need a second Pattern/Melody type flag in the row itself.

### Working material: ACCEPT / DISCARD / NEXT

- `D`: prepare the current local REVOICE development action where the Synth page owns it;
- `Enter` with NEXT ready: request GO; during PLAY activation waits for the next bar;
- `Esc`: cancel NEXT, or disarm a queued GO while keeping NEXT;
- `Alt+Enter` / `Ctrl+Enter`: ACCEPT working material;
- `Alt+Backspace`: DISCARD working edits to the accepted version;
- `Ctrl+Z`: bounded retained Undo where supported.

Two historical documentation claims are deliberately removed in 0.9.17:

- `Alt+V` is **not** a reachable Synth CONNECT shortcut because the display owns it as a hard-global navigation chord before the page sees the event;
- `Alt+X` is **not** a reachable Synth DISCARD shortcut because it is hard-global LiveMix.

The CONNECT operation may exist internally, but the current front-panel reference must not invent a key that cannot reach it.

### KNOBS / MORE

These tabs own Synth TYPE, engine parameters and supported FX/sound editing. They are not separate workflow pages.

## 5. DRUMS

The main generation controls are intentionally distinct:

```text
G             drums-only strong generation at current request level
Ctrl+G        randomize focused drum voice
Alt+G         full-pattern CHAOS
Ctrl+Alt+G    Stage 12 phrase audition/probe
P             shared request level
```

The grid uses `Q..I` for patterns, `B` for bank, arrows for the cursor, `Enter` for the selected hit and `A` for accent on an existing hit.

## 6. SONG

Song rows refer to pattern/material slots. For Synth A/B, the saved slot descriptor decides whether playback uses Pattern or Melody.

Important controls include:

- `Up/Down`: Song row;
- `Left/Right`: move across Synth A -> Synth B -> Drums;
- `Q..I`: assign an existing slot;
- `G`: generate safe material and assign the selected cell;
- double `G`: materialize Synth A + Synth B + Drums for the current row;
- `Backspace`: clear the current/selected Song cell;
- `Ctrl+N` / `Ctrl+M`: insert / remove row;
- `Alt+J`: open MATERIAL with the row as an explicit destination;
- `Alt+X`: global LiveMix, not a Song-local discard command.

A working Melody can keep sounding while being edited. ACCEPT/DISCARD belongs to the material/Synth ownership path; do not rely on the obsolete `Alt+X = DISCARD` wording from older documentation.

## 7. MATERIAL — the first-session composition page

MATERIAL exposes the generated-Phrase product workflow through:

```text
LENGTH
STYLE
TO
LAST ACCEPTED
```

The beginner model is:

```text
G  -> NEW TAKE
D  -> DEVELOP eligible fresh TAKE
R  -> MAKE ROOM when slots are exhausted
```

### First bounded development cycle

1. `Fn+M` -> MATERIAL.
2. Set `LENGTH 4B`.
3. Set `STYLE REWORK`.
4. Leave `TO APPEND` for the simplest workflow.
5. Press `G` for a fresh TAKE.
6. While that TAKE remains unedited and in the same source context, press `D`.
7. Listen through the resulting `DEVELOP + BREAK` or `BREAK ONLY` section.
8. `Ctrl+Z` removes the retained added cycle if you do not want it.

The current implementation intentionally supports a bounded first slice. It is better for the UI to say “make a fresh 4B REWORK TAKE first” than to expose semantic/depth implementation vocabulary.

Acid/House development, edited-source growth, repeated multi-cycle development and persistent development history remain outside this bounded slice unless a later release explicitly closes them.

### Make room

When `G` or `D` says `NO ROOM`, press plain `R`.

The automatic offer is limited to generated takes from the current session that:

- are no longer used by Song;
- have not been hand-edited;
- are not CURRENT/working/NEXT/Undo-protected material.

`Enter` allows those slots to be reused later; `Esc` cancels; `S` opens the expert slot-by-slot chooser. Granting permission does not erase material immediately.

## 8. MATERIAL BANK

MATERIAL BANK is the older capture/derive/write workspace and remains a separate expert page.

```text
1..4              Phrase A/B/C/D
Up/Down           1/2/4/8-bar length
Left/Right        preview saved Phrase bar
Ctrl+Left/Right   TO +/-1
Ctrl+Up/Down      TO +/-8
Enter             capture current Song region
D                 derive parent into selected slot
G                 generate connected material at TO
W                 INSERT before TO
Alt+W             REPLACE at TO
```

Fresh multi-row Phrase generation is STOP-only. Here `D` means derive, not the MATERIAL DEVELOP action.

## 9. PERFORM and external keyboard

MIDI KEYBOARD is the live performance surface. `Tab` opens PERFORMANCE TOOLS with KEY / CHORD / ARP / RHYTHM contexts.

The v0.9.16 Foundation Freeze includes the accepted USB Host external-keyboard path through PERFORM. The PROJECT MIDI setup chooses the boot USB role (`COMPUTER`, `KEYBOARD`, or `OFF`), applied across the explicit reboot boundary.

A connected keyboard feeds the existing PERFORM owner rather than bypassing it with a second note/performance engine. Accepted paths include CHORD, ARP, LATCH, rhythm and receiver mono/poly behavior documented in the release evidence.

## 10. MIDI Player and HUB MIDI

Open MIDI Player with `Alt+P`.

Core controls include:

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

Route changes invalidate stale queued events from the previous destination and perform scoped cleanup rather than forcing a global panic.

## 11. Save and Undo

Project Save/Load lives on PROJECT. It is distinct from Synth `Alt+Enter`, which ACCEPTs working material.

`Ctrl+Z` is the public global Undo chord for the current bounded retained edit. GroovePuter intentionally does not pretend to provide an unlimited DAW-style history.

After a meaningful first session, save and reload once to confirm that the project, material descriptors and Song references return together.

## 12. Help strategy

Use documentation in this order:

1. on-screen footer;
2. `Alt+H` page-aware help;
3. [`docs/user/QUICKSTART.md`](docs/user/QUICKSTART.md);
4. [`src/ui/docs/keys.md`](src/ui/docs/keys.md) for the complete key map;
5. this manual for workflow detail;
6. research/architecture documents only when investigating implementation or design history.

The goal of 0.9.17 is that a musician reaches a first useful result before needing levels 4-6.

## 13. Build / install boundary

Developer builds still use the repository scripts and CI. A normal user should not need Arduino tooling as the primary install path; 0.9.17 productization work should make the accepted release artifact installable through a user-oriented launcher/burner flow.

The public hardware-accepted foundation remains `v0.9.16`. Exact hashes, FQBN and hardware acceptance evidence belong to the GitHub release and `docs/releases/` records.

## 14. Product boundary

0.9.17 is **First Five Minutes**, not a new music-architecture cycle.

Do not reopen the Foundation Freeze for speculative lineage, genre or generation subsystems merely to improve onboarding. If first-user observation proves a specific musical or runtime gap, open the smallest evidence-backed follow-up.
