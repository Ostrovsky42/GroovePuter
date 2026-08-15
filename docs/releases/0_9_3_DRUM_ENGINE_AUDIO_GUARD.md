# GroovePuter 0.9.3 — Drum Sequencer Realtime UI Blocker

## Purpose

Close the two concrete realtime defects found while validating the Track A-only 0.9.4 candidate.

The 0.9.4 candidate changes no runtime product code, so both defects are inherited from the frozen 0.9.3 parent:

1. `GlobalDrumFeelPage` could replace the owning drum synth object while `AudioTask` was dereferencing it for every rendered sample.
2. The retained Minimal Drum Sequencer draw path rendered `grid_component_` explicitly and then called `Container::draw()`. The grid is already that container's child, so the whole drum grid was rendered a second time. The hardware log showed the first Drum Sequencer transition at `ui=339320us` with audio peak `157.6%`.

The fix stays in the existing wrapper layer:

- reuse the existing `AudioGuard` for Drum Feel **Character** events so engine replacement occurs only after AudioTask acknowledges a block boundary;
- suppress only the duplicate inherited child draw while the main Drum Sequencer tab is active;
- keep Retro/Amber, other Drum Feel rows, global navigation and the retained legacy source behavior otherwise unchanged.

No DSP algorithm, drum model, sampler path, Scene schema, MIDI route or Tape behavior changes.

## Hardware list

- M5Stack Cardputer ADV / ESP32-S3
- normal no-PSRAM GroovePuter build
- microSD with the same content used for the current release smoke
- USB-C data cable
- optional Yamaha SEQTRAK; not required for the local audio reproduction

## Wiring

No external wiring is required.

PORT.A / I2C is not used by this test.

## Build / flash

Checkout the candidate branch and record its exact SHA:

```bash
git fetch origin
git checkout agent/20260815-drum-engine-audio-guard
git pull --ff-only
git rev-parse HEAD
```

Run the focused regression:

```bash
python3 tests/test_drum_engine_audio_guard_source_regressions.py
```

Run normal release gates:

```bash
bash tests/run_host_tests.sh
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh \
  build/cardputer-adv-current/GroovePuter.ino.elf
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

Flash and monitor the same exact SHA:

```bash
bash scripts/upload.sh /dev/ttyACM0
./scripts/monitor.sh 60
```

## Expected behavior

Use the same sequence that exposed the problem:

1. cold boot and start PLAY;
2. navigate into DRUM SEQUENCER and note the first `[PERF]` after the page appears;
3. switch to the global Drum Feel/Character tab;
4. repeatedly change `808 -> 909 -> 606 -> SP12 -> 808` while PLAY continues;
5. navigate out and back to DRUM SEQUENCER once;
6. perform a short Pattern/Song and sampler smoke.

Expected:

- Drum Sequencer first draw no longer performs the duplicate grid child pass;
- the previous `ui=339320us / audio peak=157.6%` transition class should not reproduce;
- engine changes still work;
- no use-after-free style audio corruption;
- the previous engine-switch peak `221.7%` should not reproduce;
- no persistent audible lag after page entry or engine change;
- I2S underruns remain zero;
- no crash, WDT or reset;
- fixed DRAM remains within the release budget.

The existing audio block is 512 frames at 22.05 kHz, about 23.2 ms. A reported audio peak above 100% therefore means at least one render exceeded its block-time budget even if the separate I2S write-failure counter still prints `underruns=0`.

The known USB-MIDI endpoint reject storm tracked separately in issue #268 can still appear when the host-side MIDI endpoint does not drain. It began in the failing log while local audio was still in the normal ~16–22% class, so keep that issue separate from this Drum Sequencer blocker.

## Troubleshooting

### Page entry still produces a >100% audio peak

Capture the five seconds around:

```text
[NAV] ... target=5
[UI] transitionToPage ... DRUM SEQUENCER
[PERF] ...
```

Also record `ui=` and `uiPeak=`. Do not add more DSP changes unless the remaining spike has a smaller reproducible owner.

### Engine switching still raises peak sharply

Record the `[PERF]` immediately before the first engine change, then after each change. The peak is cumulative, so the acceptance signal is that engine switching does **not increase** the pre-switch peak into the previous ~221% class.

### Engine changes no longer respond

Verify the Character row is selected. Global navigation is intentionally not wrapped by the guard, and non-Character Drum Feel rows keep their existing realtime path.

### USB reject count grows while local audio is clean

Keep issue #268 separate. Record `[USB-DIAG]`, but do not modify the Drum Sequencer fix to compensate for the USB endpoint problem.

### Fixed DRAM changes materially

Reject the candidate. These changes add no persistent runtime buffer or heap owner.

## Acceptance checklist

### Software

- [ ] focused Drum Sequencer realtime UI regression green;
- [ ] full host/Core suite green;
- [ ] SDL green;
- [ ] Cardputer ADV normal compile green;
- [ ] fixed DRAM budget green;
- [ ] SEQTRAK MIDI-only compile green;
- [ ] sampler recovery regressions remain green;
- [ ] Tape resource behavior unchanged.

### Cardputer ADV

- [ ] exact SHA recorded before flashing;
- [ ] cold boot reaches normal UI;
- [ ] PLAY starts normally;
- [ ] first DRUM SEQUENCER entry does not reproduce the previous 339 ms / 157.6% class;
- [ ] navigate out and back into DRUM SEQUENCER without persistent audio lag;
- [ ] `808 -> 909 -> 606 -> SP12 -> 808` switching works while PLAY is active;
- [ ] engine switching does not increase audio peak into the previous ~221.7% class;
- [ ] no audible persistent lag after page entry or engine switching;
- [ ] I2S underruns remain `0`;
- [ ] WDT/reset/crash count remains `0`;
- [ ] heap integrity remains OK;
- [ ] short sampler smoke still works;
- [ ] Pattern/Song playback remains unchanged.

After CLEAN hardware acceptance, merge this blocker into `dev_0.9.3`, freeze the resulting new 0.9.3 SHA, then rebuild the Track A-only 0.9.4 candidate on that exact parent. Do not merge the currently blocked #279 as-is.
