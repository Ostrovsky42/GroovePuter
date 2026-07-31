# UI Transition Performance Stage

## Purpose

Reduce Cardputer-Adv page-transition stalls without changing GroovePuter musical behavior, MIDI routing, transport timing, scene data, SMF playback, or page layout.

The first confirmed hot-path issue is production verbose logging. `src/debug_log.h` previously defaulted to `DEBUG_LEVEL 4`, enabling synchronous DEBUG/INFO serial writes from UI, scene, input, and pattern code during normal hardware use. The default is now WARN (`DEBUG_LEVEL 2`). Verbose diagnostics remain opt-in with a build override such as `-DDEBUG_LEVEL=4`.

This stage intentionally does not modify DX/drum routing, SEQTRAK transport, PatternPlayer MIDI, realtime SMF playback, AudioTask ownership, or DSP behavior.

## Hardware list

- M5Stack Cardputer-Adv / ESP32-S3.
- Existing GroovePuter internal audio path.
- Optional Yamaha SEQTRAK over the existing USB MIDI connection for regression testing.
- Optional USB serial monitor at 115200 baud.

## Wiring

No wiring changes.

```text
Cardputer-Adv internal audio: unchanged
Cardputer-Adv USB-C -> SEQTRAK: unchanged when MIDI regression-testing
PORT.A: unchanged, GPIO2 SDA / GPIO1 SCL
```

## Build / Flash

Use the repository-pinned M5Stack ESP32 Arduino core `3.2.2`.

```bash
bash tests/run_host_tests.sh
bash scripts/build_sdl.sh
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
```

Normal hardware builds use the default WARN logging budget. For focused diagnostics, explicitly override the level rather than changing the repository default:

```text
-DDEBUG_LEVEL=4
```

## Expected behavior

- GroovePuter boots and all pages remain available.
- Normal navigation no longer emits routine DEBUG/INFO traffic from the logging macros.
- WARN and ERROR logs remain available.
- Internal audio, MIDI Clock/Start/Stop, Pattern MIDI, PERFORM routing and realtime SMF playback remain unchanged.
- Page transitions should spend less time blocked by synchronous serial output.
- A deliberate `DEBUG_LEVEL=4` build restores verbose diagnostics.

## Hardware performance test

Run normal playback, then repeatedly traverse the detailed-page carousel in both directions, including PERFORM, PROJECT and MIDI PLAYER.

```text
playback running
-> browse all pages repeatedly
-> exercise PROJECT and MIDI PLAYER multiple times
-> return to PERFORM
```

Pass criteria:

- no reset or watchdog;
- no transport restart;
- no audible audio/MIDI timing disruption;
- no new stuck notes;
- no increasing audio-underrun regression;
- routine DEBUG/INFO macro traffic absent in the normal build;
- transitions are at least as responsive as current main.

If one page still has a reproducible stall while the rest remain smooth, record that page as a page-specific follow-up rather than expanding this logging-budget PR.

## Known page-specific follow-up candidates

These are intentionally outside this minimal repair and should only be changed after measurement:

1. lazy page destruction/reconstruction in `MiniAcidDisplay::getPage_()`;
2. synchronous scene-directory scanning when `ProjectPage` is constructed;
3. incomplete transition timing telemetry: the existing draw timing does not cover all page construction and `onEnter()` work.

Do not increase page cache size blindly. The supported Cardputer-Adv build has no PSRAM and the firmware now also reserves long-lived AudioTask and SMF runtime memory.

## Troubleshooting

### Serial output looks quieter

Expected. The normal runtime level is WARN. Errors and warnings still print. Use an explicit DEBUG_LEVEL 3 or 4 build for detailed tracing.

### A page still pauses noticeably

Repeat the same transition in both directions while playback is running. If the pause is isolated to one page, keep this PR minimal and move the measured page-specific cause to a follow-up PR.

### Audio, MIDI or SMF timing changes

Treat that as a regression. This stage must not change AudioTask ownership, `MidiDispatchTask`, MIDI clock generation, PatternPlayer scheduling, SMF scheduling, or DSP behavior.

## Acceptance checklist

- [ ] Cardputer boots normally.
- [ ] Internal audio works.
- [ ] All original pages remain reachable with `[` / `]`.
- [ ] PERFORM remains reachable and A/B/DX/DRUMS routing is unchanged.
- [ ] MIDI PLAYER remains reachable and playback/seek still works.
- [ ] Repeated navigation does not reset or watchdog.
- [ ] Normal serial output is free of routine DEBUG/INFO macro spam.
- [ ] WARN/ERROR output still works.
- [ ] MIDI Clock/Start/Stop remains stable while navigating.
- [ ] Pattern MIDI Synth A/B remains stable while navigating.
- [ ] No new stuck notes after page transitions.
- [ ] No increasing audio-underrun regression.
- [ ] Any remaining reproducible stall is identified by page and deferred to a focused follow-up.
