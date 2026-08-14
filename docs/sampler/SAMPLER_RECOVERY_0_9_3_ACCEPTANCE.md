# GroovePuter 0.9.3 — Sampler Recovery Acceptance

## Purpose

Release-candidate acceptance for the recovered Cardputer ADV sampler stack.

0.9.3 restores the existing sampler end to end: standalone `Alt+K` page, eight sequenced drum pads, stable `SampleRef`, registry-before-Scene boot order, Save/reboot/Load persistence, and control-side WAV preload outside the audio mutation critical section.

This is recovery, not sampler productization.

## Hardware

- M5Stack Cardputer ADV / ESP32-S3
- microSD card
- USB-C data cable
- normal PSRAM-disabled GroovePuter build
- optional Yamaha SEQTRAK only when separately reproducing USB-MIDI issue #268

## Wiring

No external wiring is required for sampler acceptance.

PORT.A / I2C is not used by this test.

## Sample preparation

Create this directory at the root of the microSD card:

```text
/samples/
```

Use short reference one-shots for the final run:

- PCM WAV
- 16-bit signed PCM
- mono
- 22050 Hz

Recommended files:

```text
/samples/kick.wav
/samples/snare.wav
/samples/ch.wav
/samples/oh.wav
/samples/clap.wav
```

For 0.9.3 acceptance keep the test library flat under `/samples`. Canonical kit/subdirectory workflow is deferred to 0.9.4. Stable `SampleRef` itself remains path-derived and distinguishes different logical paths.

## Build / Flash

Use the exact final candidate SHA recorded in PR #272 (or the final stacked G PR number if it differs):

```bash
git fetch origin
git checkout agent/20260814-0.9.3-sampler-final-acceptance

git rev-parse HEAD
bash tests/run_sampler_recovery_0_9_3_tests.sh
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh \
  build/cardputer-adv-current/GroovePuter.ino.elf
bash scripts/build_seqtrak_midi_only.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

## Acceptance procedure

Run this as one continuous final acceptance on the exact candidate SHA.

1. Cold boot with the prepared SD card.
2. Confirm Serial order: SD mounted -> `[SAMPLER-REGISTRY] ready` -> Scene restore/apply.
3. Open `Alt+K` -> `SAMPLER`.
4. Assign at least PAD1 kick, PAD2 snare, PAD3 closed hat, PAD4 open hat and PAD8 clap.
5. Trigger PAD1..PAD8 directly with `Q W E R T Y U I`.
6. Press `Space` to prelisten the selected pad.
7. Start `PLAY` with an existing drum pattern and confirm the drum sequencer triggers the assigned samples.
8. Change pitch on one pad and confirm the sequenced result changes audibly.
9. Enable reverse on another pad and confirm reverse is heard from the sequencer.
10. Put closed/open hat in the same non-zero choke group and confirm choke behavior.
11. Set non-default volume, start, end and loop values and smoke-test them.
12. While `PLAY` is active, change `SMP` on a pad. Record `audioUnderruns` before/after. The control-side load may take time, but audio must not freeze because AudioGuard is held across WAV I/O/allocation.
13. Save explicitly through PROJECT.
14. Hard power-cycle the Cardputer ADV.
15. Load the same project/Scene.
16. `PLAY` again and verify the same physical WAV assignments plus volume, pitch, start/end, reverse, loop and choke.
17. Power off, rename/remove one referenced WAV, boot once and verify the project still loads without WDT/crash or wrong-file substitution. The missing pad may remain silent/unresolved.
18. Restore the WAV and boot normally.
19. Run a 30-minute mixed sampler soak: Song/Pattern playback, Sampler page navigation, direct/prelisten triggers, practical 1/4/8-voice activity, one Save, and repeated Stop/Play cycles.

## Expected screen behavior

- `Alt+K` opens standalone `SAMPLER` without changing workflow cycling.
- `Up/Down` changes the focused field.
- `Left/Right` changes PAD/SMP or the selected parameter.
- user PAD selection is bounded to pads 1..8; internal pads 9..16 are not a 0.9.3 workflow.
- `Q W E R T Y U I` trigger pads 1..8.
- `Space` prelistens the selected pad.
- fields remain available for `SMP`, `VOL`, `PCH`, `STR`, `END`, `LOP`, `REV`, `CHK`.
- historical KIT LOAD is not exposed in 0.9.3; it is deferred rather than left unsafe.

## Expected Serial behavior

Boot must show registry readiness before Scene restore, for example:

```text
[SD] mount result=1 ...
SampleIndex::scanDirectory: Scanning '/samples'...
SampleIndex: Found: /samples/kick.wav ...
[SAMPLER-REGISTRY] ready discovered=... registered=...
...
MiniAcid::init: Loading scene from storage...
...
applySceneStateFromManager
```

A valid restored pad may then log `Preload: Loading ...`.

During interactive sample changes, preload failure must produce a `[SAMPLER] sample assignment rejected: ...` diagnostic and keep the previous pad assignment.

A missing stable reference must not select another file by basename/legacy hash.

## Memory evidence table

Fill this table from the exact final candidate hardware run. Do not copy older numbers into these cells.

| Checkpoint | freeInt | largestInt | free8 | largest8 | sampler pool used / capacity | resident samples |
|---|---:|---:|---:|---:|---:|---:|
| boot baseline | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING |
| registry ready | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING |
| Scene loaded | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING |
| reference samples resident | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING |
| PLAY / sampler active | PENDING | PENDING | PENDING | PENDING | PENDING | PENDING |

Also record from the exact build:

```text
Global variables: PENDING B
Fixed DRAM gate: PENDING
ELF fingerprint: PENDING
```

If normal product telemetry does not expose pool-used/resident-count values, record `N/A (no product telemetry)` rather than inventing values. Pool policy must remain 32 KiB in 0.9.3.

## Realtime evidence

Record for the exact final hardware run:

```text
baseline audioUnderruns: PENDING
post live-SMP-change audioUnderruns: PENDING
30-minute soak audioUnderruns: PENDING
peak audio CPU: PENDING
WDT/reset: PENDING
stuck audio: PENDING
```

Acceptance is zero WDT/reset/UAF/stuck-audio events and no systematic underrun increase caused by sampler recovery.

## Persistence evidence

The final hardware run must prove:

```text
assign WAV
change VOL/PCH/STR/END/REV/LOP/CHK
SAVE
hard power cycle
LOAD
same SampleRef -> same physical WAV
same parameters
PLAY -> same sample audible
```

Then remove one referenced WAV for one boot and verify fail-closed missing-sample behavior.

## Troubleshooting

- `discovered=0`: confirm `/samples` exists at the SD root and contains supported `.wav` files directly in that directory.
- sample selection rejected: inspect the `[SAMPLER]` diagnostic; the previous pad assignment should remain unchanged.
- `Preload: ID ... not found in registry`: confirm registry-ready appears before Scene load and that the file still exists.
- wrong file after rename/missing sample: FAIL; stable identity must not fall back to another basename match.
- long UI pause but audio continues: measure it; control-side synchronous preload is allowed, AudioGuard-held audio freeze is not.
- USB-MIDI reject/blocking with no SEQTRAK connected belongs to #268 reproduction and does not change sampler acceptance unless it reproduces with the device connected.
- fixed DRAM failure: stop; do not increase/decrease sampler pool to hide the failure.

## Acceptance checklist

### Software

- [ ] `tests/run_sampler_recovery_0_9_3_tests.sh` PASS.
- [ ] `tests/run_host_tests.sh` PASS.
- [ ] SDL PASS.
- [ ] Cardputer ADV normal PASS.
- [ ] Cardputer ADV fixed DRAM PASS.
- [ ] SEQTRAK MIDI-only PASS.
- [ ] Phrase Core PASS.
- [ ] Four-axis PASS.
- [ ] Synth persistence PASS.
- [ ] Stage 15 existing matrices PASS.

### Cardputer ADV

- [ ] exact final SHA recorded.
- [ ] registry ready before Scene restore.
- [ ] PAD1/2/3/4/8 WAV assignments work.
- [ ] Q/W/E/R/T/Y/U/I direct triggers cover pads 1..8.
- [ ] Space prelisten works.
- [ ] sequencer triggers sampler.
- [ ] pitch/reverse/choke audible.
- [ ] volume/start/end/loop smoke passes.
- [ ] live sample change does not hold AudioGuard across WAV loading.
- [ ] Save -> hard reboot -> Load preserves physical WAV + parameters.
- [ ] missing referenced WAV boots fail-closed without wrong substitution.
- [ ] final memory table recorded.
- [ ] 30-minute soak: 0 WDT/reset/UAF/stuck audio/systematic underruns.

## Deferred to 0.9.4

- sampler productization
- canonical/transactional kit format and kit admission/rollback
- recursive kit workflow
- memory arena
- SD streaming
- WAV loader redesign / chunked stereo conversion
- SYNTH/SAMPLE/LAYER
- waveform trim/editor
- slicing
- round robin
- recording
- polished missing-sample relink UX
- Tape / Voice / Recorder recovery
- external synth profiles
