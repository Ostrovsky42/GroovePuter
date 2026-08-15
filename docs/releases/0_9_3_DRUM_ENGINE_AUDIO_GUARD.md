# GroovePuter 0.9.3 — Drum Engine AudioGuard Blocker

## Purpose

Close the hardware-only realtime race found while validating the 0.9.4 Track A candidate.

The 0.9.4 candidate adds no runtime product code, so the defect belongs to the frozen 0.9.3 parent. `GlobalDrumFeelPage` changed the owning drum synth object while `AudioTask` could still call that object for every rendered sample.

The fix is deliberately narrow: when the Drum Sequencer **Character** row handles a key event, reuse the page's existing `AudioGuard` so the drum engine replacement happens only after `AudioTask` acknowledges a block boundary.

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

Start PLAY, open Drum Sequencer, switch to the global Drum Feel/Character tab, then repeatedly change the engine among 808, 909, 606, CR78, KPR77 and SP12.

Expected:

- engine changes still work;
- no crash, WDT or reset;
- no use-after-free style audio corruption;
- no persistent audible lag after an engine change;
- audio peak should not reproduce the previous `157.6% -> 221.7%` engine-switch spike class;
- I2S underruns remain zero;
- fixed DRAM remains within the release budget.

The known USB-MIDI endpoint reject storm tracked separately in issue #268 can still appear when the host configures the MIDI interface but does not drain it. That issue is not evidence that this AudioGuard fix failed. For this blocker, judge the local audio path and engine-switch timing separately.

## Troubleshooting

### Audio peak still exceeds 100% before any engine switch

Capture the preceding 5 seconds of `[KEY]`, `[UI]` and `[PERF]` lines. The original hardware log also contained one `157.6%` peak in the navigation/generation window before the repeated engine changes. If that remains after this race is removed, treat it as a second blocker rather than widening this fix blindly.

### Engine changes no longer respond

Verify the Character row is selected. Global navigation is intentionally not wrapped by this guard, and non-Character Drum Feel rows keep their existing realtime path.

### USB reject count grows while local audio is clean

Keep issue #268 separate. Record `[USB-DIAG]`, but do not modify the audio-guard fix to compensate for a host that is not draining the USB-MIDI endpoint.

### Fixed DRAM changes materially

Reject the candidate. This change should add no persistent runtime buffer or new heap owner.

## Acceptance checklist

### Software

- [ ] focused Drum engine AudioGuard regression green;
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
- [ ] Drum Sequencer opens normally;
- [ ] 808 -> 909 -> 606 -> SP12 -> 808 switching works while PLAY is active;
- [ ] no audible persistent lag after switching;
- [ ] no audio peak in the previous `>150% / >220%` engine-switch class;
- [ ] I2S underruns remain `0`;
- [ ] WDT/reset/crash count remains `0`;
- [ ] heap integrity remains OK;
- [ ] short sampler smoke still works;
- [ ] Pattern/Song playback remains unchanged.

If the engine-switch spike is fixed but a separate `>100%` peak still appears around `G` or page navigation, stop and capture that smaller reproduction. Do not merge this blocker and call the entire hardware lag solved until the remaining spike is understood.
