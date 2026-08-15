# GroovePuter 0.9.3 — Sampler DRUMS Workflow UX

## Purpose

Productize the recovered sampler without redesigning it.

The previous standalone `Alt+K` page exposed useful recovery controls but behaved like a debug surface. SAMPLES now belongs to the existing DRUMS tab cycle and adds two distinct OFF contracts:

- **Layer OFF** — temporarily bypass all sample playback while preserving pad assignments.
- **Pad sample OFF** — explicitly clear the WAV assignment from one selected pad.

No sampler DSP, WAV loader, cache policy, Scene schema, drum sequencer timing, Tape runtime or MIDI routing is changed.

## Hardware list

- M5Stack Cardputer ADV / ESP32-S3
- normal no-PSRAM GroovePuter build
- microSD card with indexed PCM16 WAV samples
- USB-C data cable for flash/serial monitor

## Wiring

No external wiring is required.

The test does not use PORT.A / I2C. Cardputer ADV remains on its normal internal audio and SD wiring.

## Build / flash

Use one exact candidate SHA throughout the test:

```bash
git fetch origin
git checkout agent/20260815-sampler-drum-workflow-ux
git pull --ff-only
git rev-parse HEAD
```

Run the focused contract and normal release gates:

```bash
python3 tests/test_sampler_page_navigation_source_regressions.py
bash tests/run_host_tests.sh
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh \
  build/cardputer-adv-current/GroovePuter.ino.elf
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

Flash and monitor the same SHA:

```bash
bash scripts/upload.sh /dev/ttyACM0
./scripts/monitor.sh 90
```

## Expected behavior

Open **DRUMS** and press `Tab` through the local pages. The cycle includes **SAMPLES** after the existing Grid / Feel / Automation pages.

Inside SAMPLES:

- `Up/Down` moves between fields.
- `Left/Right` changes the selected value or WAV.
- `Q W E R T Y U I` auditions pads 1..8 only while SAMPLES is open.
- `Enter` previews the current pad.
- `Space` remains the global transport key.
- `M` toggles the complete sample layer `ON/OFF`.
- Layer `OFF` stops current sample voices and prevents new sample playback, but keeps every pad→WAV assignment.
- Turning the layer `ON` restores sample playback from the same assignments.
- On the `SAMPLE` row, `Backspace/Delete` clears only the current pad assignment and shows `OFF`.
- Empty pads do not trigger sample playback; the synthesized drum engine is unaffected.
- The layer row shows `ON/OFF` plus the number of assigned pads, for example `ON  3/8`.
- Long WAV filenames are compacted instead of dominating the screen.

Legacy page id 15 remains decodable, but normal navigation resolves it to DRUMS. `Alt+K` is no longer advertised as a standalone sampler workflow.

## Troubleshooting

### M disables samples but drum synthesis also disappears

Reject the candidate. The layer toggle must affect only `DrumSamplerTrack`; 808/909/606/CR78/KPR77/SP12 synthesis must continue normally.

### Sample returns after clearing the pad

Verify `Backspace/Delete` was pressed while the `SAMPLE` row was focused. A cleared pad must display `OFF`, mark the Scene dirty, and remain empty after normal Scene Save/Load.

### M OFF loses assignments

Reject the candidate. Master OFF is a non-destructive runtime bypass. It must not rewrite pad IDs or unload the Scene mapping.

### Space previews a sample instead of controlling transport

Reject the candidate. Preview moved to `Enter`; SAMPLES must not own `Space`.

### DRUMS Tab cycle has only three pages

Confirm the exact SHA and ensure `src/ui/pages/drum_sequencer_page.cpp` lazily attaches `SamplerPage` before the first Tab event.

### Audio stutters when opening SAMPLES

Capture `[PERF]`, `[UI]`, `[HEAP]` and `[WDT]` lines. The page is intentionally lightweight and creates its controls lazily; no WAV decode or preload should happen merely by opening the tab.

## Acceptance checklist

### Software

- [ ] sampler DRUMS workflow source regression passes;
- [ ] full host/Core suite passes;
- [ ] SDL build passes;
- [ ] Cardputer ADV normal build passes;
- [ ] fixed DRAM gate passes;
- [ ] SEQTRAK MIDI-only build passes;
- [ ] sampler recovery / registry / persistence regressions remain green;
- [ ] no Tape or MIDI routing behavior changes.

### Cardputer ADV

- [ ] exact SHA recorded before flashing;
- [ ] cold boot reaches normal UI;
- [ ] DRUMS `Tab` cycle reaches SAMPLES and returns to Grid;
- [ ] selecting WAVs on at least two pads works;
- [ ] `M` OFF immediately silences only sample playback;
- [ ] `M` ON restores the same sample assignments;
- [ ] `Backspace/Delete` on SAMPLE clears only the selected pad;
- [ ] cleared pad remains empty after normal Scene Save/reload;
- [ ] `Enter` previews the selected sample;
- [ ] `Space` still starts/stops transport from SAMPLES;
- [ ] Q-I audition works only as local SAMPLES pad audition;
- [ ] synthesized drums continue while sample layer is OFF;
- [ ] no new audible lag, WDT, reset or crash;
- [ ] I2S underruns remain `0` during a short playback smoke;
- [ ] heap integrity remains OK.
