# Song playhead repair stage

## Purpose

Correct Song row duration so `patternBars` is interpreted as musical bars. The audio engine invokes the Song-cycle callback once per bar; it must not multiply that callback count by the 16 sequencer steps.

## Root cause

The previous implementation called `advanceSongStep_()` only at `barTick == 0`, but advanced the row after `SEQ_STEPS * patternBars` calls. This made a `1B` row last 16 bars, a `2B` row last 32 bars, and so on.

## Required behavior

```text
1B -> advance after 1 complete bar
2B -> advance after 2 complete bars
4B -> advance after 4 complete bars
8B -> advance after 8 complete bars
```

The first boundary after Start establishes bar 1 and must not immediately change the row. Start, Stop, Song-mode changes, manual row selection, playback-slot changes, LiveMix changes, and scene application restart the row phase.

## Implementation

- `songStepCounter_` is replaced by `songBarIndex_` with an explicit pre-first-bar value of `-1`;
- `advanceSongBar_()` consumes one callback per musical bar;
- `nextSongCycleBoundary()` contains the deterministic 1/2/4/8-bar transition rule;
- `cycleBarIndex()` exposes the current bar directly without sixteenth-step conversion;
- reverse, loop-range, rehearsal rows, Pattern mode, internal audio, and current USB-MIDI routes remain in their existing paths.

## Scope

- replace the legacy sixteenth-step counter with an explicit bar index;
- preserve reverse, loop-range, rehearsal-row, pattern-mode, internal-audio, and USB-MIDI behavior;
- add deterministic host tests and Cardputer-Adv acceptance documentation;
- use the repository-pinned M5Stack ESP32 core `3.2.2`.

## Out of scope

- USB-MIDI jitter repair;
- drum MIDI;
- MIDI Clock or transport messages;
- Song UI redesign;
- scene schema changes.

## Merge gate

- [x] focused host tests pass
- [ ] full pull-request CI passes on the final head
- [ ] SDL build passes
- [ ] Cardputer-Adv build passes with core `3.2.2`
- [ ] `1B`, `2B`, `4B`, and `8B` durations pass on hardware
- [ ] Stop/Play and manual row selection restart the row phase
- [ ] reverse and loop behavior remain correct
- [ ] internal Synth A, Synth B, and drums remain audible
- [ ] no reset, watchdog, or continual underrun growth
