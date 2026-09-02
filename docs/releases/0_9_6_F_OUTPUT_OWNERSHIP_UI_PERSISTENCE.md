# GroovePuter 0.9.6-F — Output Ownership UI + Scene Persistence

## Purpose

Expose the canonical track output owner on the existing Synth A, Synth B and DRUMS pages and persist it with the project Scene.

User-visible states are exactly:

```text
INTERNAL
MIDI
LAYER
```

There is no fourth visible legacy mode. Old Scenes without the new field keep the <=0.9.5 source-dependent behavior until the user makes an explicit output choice.

## Hardware list

- M5Stack Cardputer ADV / ESP32-S3
- Yamaha SEQTRAK for external MIDI acceptance
- USB-C data cable
- microSD card for normal project Save/Load
- optional short `/samples/*.wav` one-shots for DRUMS sampler-layer acceptance

## Wiring

No new wiring is introduced.

Cardputer ADV PORT.A remains unchanged:

```text
SDA GPIO2
SCL GPIO1
```

Output ownership uses the existing USB-MIDI path to SEQTRAK. It does not use PORT.A, BLE, ESP-NOW or a new USB Host architecture.

## UI

On **SYNTH A**, **SYNTH B** or any **DRUMS** sub-page:

```text
Alt+O
```

changes the owning track output.

For a project that already has an explicit mode:

```text
INTERNAL -> MIDI -> LAYER -> INTERNAL
```

For a legacy project with no persisted OutputMode, the first `Alt+O` deliberately selects:

```text
LAYER
```

This is the first explicit user choice. Merely loading an old Scene does not silently change its accepted <=0.9.5 behavior.

The UI reports the new state with a toast:

```text
SYNTH A OUT:INTERNAL
SYNTH B OUT:MIDI
DRUMS OUT:LAYER
```

Every successful change:

1. runs through the page's existing `AudioGuard`;
2. uses the 0.9.6-E transition cleanup helper;
3. marks the project Scene dirty.

The existing Sampler page control `LAYER: ON/OFF` is **not** renamed or reused. That control remains the internal sample-source layer. `DRUMS OUT:LAYER` means local Drums sources plus external MIDI.

## Persistence contract

Output state is project Scene data, not device/global MIDI settings.

A compact top-level extension is written:

```json
"out":[A,B,D]
```

Each element is bounded to:

```text
0 = hidden legacy compatibility
1 = INTERNAL
2 = MIDI
3 = LAYER
```

`0` is persistence-only compatibility state and is never presented as a fourth OutputMode.

### Cardputer ADV

The existing transactional sampler Scene streaming boundary is reused. No second whole-Scene buffer is added.

Write path:

```text
SceneManager JSON
  -> existing SampleRef streaming filter
  -> OutputMode root-field injector
  -> existing transactional .tmp / .bak Scene commit
```

Read path:

```text
SD Scene
  -> capture OutputMode without mutating runtime
  -> existing SampleRef streaming filter
  -> SceneManager validation
  -> only after successful parse: commit captured OutputMode
```

An invalid main Scene therefore cannot install its OutputMode before the existing backup fallback is attempted.

### SDL

Main Scene and autosave use the same semantic ordering:

```text
capture output state
-> SceneManager load succeeds
-> commit output state
```

### Legacy / malformed files

- no `out` field -> restore hidden legacy compatibility;
- valid `[0..3,0..3,0..3]` -> exact restore;
- nested or string `"out"` does not count as the root field;
- duplicate root `out`, wrong element count, non-bounded value or non-array -> fail closed;
- route/channel/device profile/connection state is not stored in this field.

## Build / Flash steps

Focused ownership gate:

```bash
bash tests/run_output_ownership_tests.sh
```

Full software gates:

```bash
bash tests/run_host_tests.sh
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh build/cardputer-adv-current/GroovePuter.ino.elf
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

Flash exact accepted candidate only:

```bash
git rev-parse HEAD
bash scripts/upload.sh /dev/ttyACM0
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

## Expected behavior

### Legacy Scene

Before the first explicit `Alt+O`:

```text
Pattern/Song Synth A/B  local + MIDI
PERFORM Synth A/B       MIDI only
Pattern Drums           local drum + enabled sample layer + MIDI
PERFORM Drums           MIDI only
Sampler preview         internal only
```

This is intentional compatibility behavior.

### Explicit INTERNAL

For the selected Synth A/B/Drums track:

- new local triggers are allowed;
- new external MIDI NoteOn is blocked;
- cleanup NoteOff/AllNotesOff still reaches old external owners if needed.

### Explicit MIDI

- new local triggers are blocked;
- new external MIDI NoteOn is allowed;
- existing sampler assignments and sampler enable state remain intact.

### Explicit LAYER

- local side and external MIDI side both participate;
- Drums local side still respects the independent sampler source-layer ON/OFF state.

## Troubleshooting

### `Alt+O` does nothing

Use it on SYNTH A, SYNTH B or DRUMS. `Alt+O` is intentionally track-page scoped; it is not a global MIDI setting.

### First press on an old project shows LAYER

Expected. The old project has hidden compatibility state rather than a canonical output mode. The first press is the explicit migration choice.

### MIDI mode clears sample assignments

Reject the build. Output ownership must never clear SampleRef/pad assignments, scan SD, preload WAVs or toggle sampler source enable.

### A stuck external note appears after switching to INTERNAL

Reject the build. Stage E requires target-scoped cleanup through the existing MIDI dispatcher and stale queued NoteOn rejection.

### Scene with one bad `out` value falls back silently

Malformed output persistence is intentionally fail-closed at the Scene level. It must not guess another mode. Normal old Scenes with no field remain fully supported.

### USB device is disconnected

OutputMode must not change. Connection state remains independent. Reconnect and verify the existing transport cleanup/recovery path.

## Acceptance checklist

- [ ] `Alt+O` works on Synth A;
- [ ] `Alt+O` works on Synth B;
- [ ] `Alt+O` works from every DRUMS tab;
- [ ] legacy first press selects LAYER;
- [ ] explicit cycle is `INTERNAL -> MIDI -> LAYER -> INTERNAL`;
- [ ] output change uses existing AudioGuard;
- [ ] output change uses Stage E cleanup;
- [ ] output change marks Scene dirty;
- [ ] toast identifies track and mode;
- [ ] Sampler page `LAYER: ON/OFF` keeps its old source-layer meaning;
- [ ] Cardputer main Save writes project output state;
- [ ] Cardputer autosave writes project output state;
- [ ] Cardputer main/backup load commits output only after valid Scene parse;
- [ ] SDL main Save/Load round-trips output state;
- [ ] SDL autosave round-trips output state;
- [ ] Scene without `out` restores legacy compatibility;
- [ ] malformed/duplicate `out` fails closed;
- [ ] mute remains orthogonal;
- [ ] MIDI route/channel/profile remains orthogonal;
- [ ] connection state remains orthogonal;
- [ ] focused output ownership gate PASS;
- [ ] full Core/host PASS;
- [ ] SDL PASS;
- [ ] Cardputer ADV normal compile PASS;
- [ ] fixed DRAM gate PASS;
- [ ] SEQTRAK MIDI-only compile PASS;
- [ ] final hardware live-switch + Save/reboot/Load acceptance PASS before merge.
