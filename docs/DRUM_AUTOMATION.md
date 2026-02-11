# Drum Automation and Groove Override

Source:
- `scenes.h`
- `scenes.cpp`
- `src/dsp/miniacid_engine.cpp`
- `src/ui/pages/drum_automation_page.cpp`
- `src/ui/pages/drum_sequencer_page.cpp`

## Purpose
This feature adds per-pattern drum automation lanes and per-pattern groove overrides:
- automate drum bus parameters over 16 steps
- switch drum engine by step
- override swing/humanize on a single drum pattern (without touching global genre feel)

## Data Model
`DrumPatternSet` now includes:
- `lanes[4]`
- `groove` (`PatternGroove`)

Lane targets:
- `0`: Reverb Mix
- `1`: Compression
- `2`: Transient Attack
- `3`: Engine Switch
- `255`: Off

Lane node format:
- `step` `0..15`
- `value` `0.0..1.0`
- `curveType`:
  - `0`: Linear
  - `1`: EaseIn
  - `2`: EaseOut

Groove override:
- `swing = -1.0` means use global
- `humanize = -1.0` means use global
- otherwise clamped:
  - `swing: 0.0..0.66`
  - `humanize: 0.0..1.0`

## Scene JSON
Drum patterns are stored as objects:

```json
{
  "v": [ /* 8 drum voices */ ],
  "lanes": [
    { "t": 0, "n": [ { "s": 0, "v": 0.2, "c": 0 }, { "s": 12, "v": 0.8, "c": 2 } ] },
    { "t": 1, "n": [ { "s": 0, "v": 0.1, "c": 0 } ] }
  ],
  "grv": { "sw": 0.22, "hz": 0.35 }
}
```

Compatibility:
- legacy drum patterns (voice array only) still load
- `lanes` and `grv` are optional

## Runtime Behavior
Automation is evaluated once per step in the audio engine:
- Reverb Mix -> updates drum reverb mix
- Compression -> updates one-knob compressor amount
- Transient Attack -> updates transient shaper attack
- Engine Switch -> maps value to `808/909/606`

Engine switch safety:
- engine is not recreated if the selected engine is already active

Groove override in timing:
- step duration now uses base timing + per-pattern drum override
- drum pattern can push/pull groove independently of global genre feel

## UI Workflow
`Drum Sequencer` now has 3 subpages:
- Main
- Global Settings
- Automation

Navigation:
- `Tab`: cycle subpages (`Main -> Settings -> Automation -> Main`)

Automation page controls:
- `Up/Down`: select row
- `Left/Right`: adjust value
- `N`: add node
- `X` or `Backspace/Delete`: remove node
- `Enter` on `SWING`/`HUMAN`: reset to `AUTO` (`-1`)

## Performance Notes (Cardputer)
- engine switch lane is expensive if changed every step; use sparse nodes
- dense retrigs/rolls + heavy FX can cause underruns on ESP32-S3
- recommended: automate `COMP`/`REV MIX` first, keep engine switching for section boundaries
