# GroovePuter 0.9.1 — Release Record

## Purpose

Freeze the shipped 0.9.1 runtime contract, record the final hardware-tested runtime
checkpoint, and separate the release from older 0.9/PR-#131 gate documents.

This document is normative for 0.9.1. `PRE_0_9_RELEASE_GATE.md` and
`0_9_FINAL_ACCEPTANCE.md` are retained only as historical 0.9 evidence.

## Runtime freeze

```text
branch:       dev_0.9.1
runtime SHA:  170bbe1407daf37621949301a34a5ec345844b24
final runtime PR: #260
#260 head:   4cd8244b091e748ddf93819a03c051d978e01266
#260 merge:  170bbe1407daf37621949301a34a5ec345844b24
```

`170bbe1407...` is the production/runtime freeze. Release-documentation commits may
follow it, but the `v0.9.1` tag must not include additional production behavior beyond
this frozen runtime unless a release-correctness blocker is discovered and the full
acceptance cycle is repeated.

## Hardware

- M5Stack Cardputer ADV / ESP32-S3;
- PSRAM disabled for the normal release profile;
- built-in display, keyboard and audio path;
- USB-C data connection for flash, Serial and USB MIDI;
- optional Yamaha SEQTRAK for external MIDI acceptance.

No external GPIO wiring is required for the standard release build.

## Automated acceptance

The final runtime candidate `4cd8244b091e748ddf93819a03c051d978e01266`
completed the current release-facing GitHub Actions matrix successfully before merge.
The green set included:

- Core regressions;
- Phrase Core;
- Synth persistence;
- Tonal Projector;
- Generation Stage 15B;
- Generation Stage 15C;
- Scale quantization correctness;
- Stage 15 Tonal Materializer;
- Stage 15 tonal baseline;
- Stage 15 tonal integration;
- Stage 15 tonal register sweep;
- Stage 15 tonal global scale;
- Stage 15 final tonal acceptance.

PR #260 additionally recorded successful Cardputer ADV normal and SEQTRAK MIDI-only
build/resource validation for the HUD-only delta.

## Hardware acceptance

The owner hardware-tested PR #260 on its exact final head
`4cd8244b091e748ddf93819a03c051d978e01266` before merge.

The accepted final HUD behavior is:

- waveform moves during PLAY;
- waveform does not leave stale pixels on partial-redraw pages;
- waveform is composited beneath mute/activity digits;
- mute digits remain readable as the topmost layer;
- MIDI Player progress waveform reacts clearly to accepted NoteOn activity.

The integrated 0.9.1 line also carries the previously hardware-accepted Stage 15,
P-level, Song/Phrase, persistence and MIDI/SEQTRAK recovery checkpoints that were
merged into `dev_0.9.1` before the final HUD freeze.

## Shipped workflow

The active workflow is:

```text
PERFORM:  MIDI KEYBOARD -> MIDI PLAYER
GENERATE: GENRE -> FEEL
HUB:      OVERVIEW -> SYNTH A -> SYNTH B -> DRUMS
SONG:     SONG -> PHRASE CORE
SETTINGS: PROJECT / SETUP
```

Active page count: **11**.

Compatibility aliases:

- persisted `GENERATION` -> FEEL;
- persisted `TEXTURE` -> FEEL;
- persisted standalone Synth A SOUND -> SYNTH A;
- persisted standalone Synth B SOUND -> SYNTH B.

Those aliases are decode/navigation compatibility only; they are not live release pages.

## Generation contract

### GENRE

- plain `G` is explicit full Stage 15 generation;
- `P` cycles `P1 CANON -> P2 VAR -> P3 TRANS`;
- repeated accepted `G` uses a bounded session reroll attempt while preserving the
  selected Genre/Variant/P-level composition identity;
- STOP generation commits immediately;
- PLAY generation prepares off the sounding bar and publishes at the next real
  `BAR_START` without transport stop/restart.

### FEEL

FEEL owns timing and velocity only: timing profile, swing, bounded feel amount,
velocity variation, repeat cycle and FEEL presets. It shares the P1/P2/P3 request
selector but does not own notes, harmony or timbre.

### Synth Notes

Plain `G` outside NOTE ENTRY rerolls only the selected Synth A/B lane through the
active Genre/recipe/P-level/harmony identity. During PLAY it publishes at `BAR_START`.
The neighboring synth and drums remain unchanged. In NOTE ENTRY, `G` remains a note.

### Drums

```text
G             drums-only strong generation at current P-level
Ctrl+G        focused drum voice randomize
Alt+G         explicit CHAOS
Ctrl+Alt+G    Stage 12 phrase audition/probe
P             P1/P2/P3 selector
```

P3 is not CHAOS.

## Synth UI contract

Synth A/B each expose one parent-owned tab cycle:

```text
[N]KM  NOTES
N[K]M  KNOBS
NK[M]  MORE
```

Standalone SOUND pages are not part of the active workflow. The bottom performance
HUD is a reserved layout region and must not cover NOTES, KNOBS or MORE content.

## Song and Phrase contract

### Song

- horizontal edit navigation crosses Song A/B only at the outer Synth A/Drums edges;
- `PAT:A/B` is independent assignment context;
- `B` changes assignment context without Song mutation;
- `Alt+B` changes stored-reference/selection bank;
- `Ctrl+B` changes playback Song slot;
- generation uses safe copy-on-write pattern allocation.

### Phrase Core

Phrase owns one visible `TO:` coordinate:

```text
Ctrl+Left/Right   TO +/-1 row
Ctrl+Up/Down      TO +/-8 rows
G                 fresh connected 1/2/4/8B generation at TO
W                 INSERT saved Phrase before TO, shift following rows
Alt+W             REPLACE Phrase lanes at TO, no row shift
```

Fresh multi-row Phrase generation is intentionally STOP-only. During PLAY it rejects
with `STOP PLAYBACK FOR PHRASE` rather than stopping/restarting transport implicitly.
Non-Phrase Song lanes are preserved, and occupied Phrase lanes reject fresh `G`
without silent overwrite.

Phrase storage remains `REFERENCE VIEW / REF MUTABLE`.

## Performance contract

PERFORMANCE TOOLS are opened with `Tab` from MIDI KEYBOARD.

```text
1  ARPEGGIATOR
2  DIRECTION
3  CHORD
4  MEMORY
5  STRUM
6  RATCHET
7  EUCLIDEAN
8  ROTATE
9  receiver MONO/POLY
-/+ velocity 10..120
```

Receiver MONO/POLY is external-MIDI ownership. SEQTRAK Synth/DX targets use the
receiver-mode MIDI path; internal Synth A/B remain sequencer/pattern instruments.

## MIDI Player / HUB MIDI contract

- MIDI Player physical mute hotkeys: `1..9`;
- `U` opens physical mute mixer;
- `H` moves between Player and HUB MIDI while preserving the loaded session;
- HUB MIDI `Up/Down` selects projected physical layer;
- HUB MIDI plain `Left/Right` changes route immediately, including during PLAY;
- HUB MIDI `Fn+Left/Right` adjusts physical-track level;
- `Enter` and `1..9` mute/unmute;
- `S` solos the selected layer;
- explicit route overrides persist per matching file identity;
- route revision filtering drops stale queued old-route events;
- scoped cleanup NoteOff prevents stuck notes without global panic.

RAW routing does not accept explicit SEQTRAK destinations.

## Persistence contract

0.9.1 retains the accepted persistence boundary:

- project-scoped pattern storage;
- Scene Save/Load;
- independent Synth A/B TYPE and visible parameter persistence;
- safe legacy decode/defaults;
- Song references;
- Phrase Core state;
- supported UI session state;
- loaded synth patch ownership wins over hidden genre timbre replacement.

## Build and flash

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

## Final release smoke

- [ ] boot and all five workflows responsive;
- [ ] GENRE `G` during PLAY changes complete material only at next BAR_START;
- [ ] P1/P2/P3 are selectable and P3 remains related rather than CHAOS;
- [ ] Synth A/B `G` changes only selected lane outside NOTE ENTRY;
- [ ] Drums `G`, `Ctrl+G`, `Alt+G` remain separate;
- [ ] Song A/B navigation and PAT context remain independent;
- [ ] Phrase `TO:`, `G`, `W`, `Alt+W`, `Ctrl+Arrow` work;
- [ ] Save -> reboot -> Load preserves synth/pattern/Song/Phrase state;
- [ ] MIDI Player 1..9 mute and HUB live route changes work;
- [ ] PERFORMANCE `9` MONO/POLY and `-/+` velocity work with SEQTRAK target;
- [ ] no stuck notes after route/mute/panic transitions;
- [ ] waveform leaves no stale pixels and does not cover mute digits;
- [ ] no watchdog reset, Guru Meditation or monotonic runtime failure.

## Known deferred

These are not 0.9.1 release gates:

- Harmony Atlas research/runtime vocabulary expansion beyond the admitted current set;
- Melodic Corpus / later melodic grammar research;
- additional chord-quality/polyphonic internal rendering work;
- broader Phrase Arranger work beyond current Phrase Core + Phrase-to-Song workflow;
- BLE MIDI;
- Tape/Sampler UI expansion;
- broad dead-code/framework cleanup;
- oversampling/wavetable mipmap experiments;
- unrelated large DSP/loudness redesigns;
- post-release UX experiments that do not fix a concrete 0.9.1 correctness defect.

## Release boundary

After the documentation-only release PR is merged, freeze the resulting exact SHA.
Run the normal release CI on that SHA and create tag **`v0.9.1`** only if no
release-correctness defect is found.

A new feature, research admission, architectural cleanup or cosmetic refinement after
this point belongs to a later release line, not to 0.9.1.
