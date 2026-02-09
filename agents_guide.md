# MiniAcid Agent Guide

Practical guardrails for contributors working in this repo.

## Platform Constraints
- Target: **M5Stack Cardputer (ESP32-S3)**
- No PSRAM assumption for release path
- Audio path is real-time: no allocations or blocking I/O in audio callback

## Architecture Snapshot
- DSP core: `src/dsp/*`
- UI pages: `src/ui/pages/*`
- Scene persistence: `scenes.cpp`, `scenes.h`
- Groove corridors: `src/dsp/groove_profile.*`

## Current UX Conventions
- Global style: `Alt+\\` (`CARBON/CYBER/AMBER`)
- Selection v2 (Pattern/Drum/Song):
  - `Ctrl + Arrows` expand selection
  - `Ctrl+C` copy and lock frame
  - Arrows move locked frame
  - `Ctrl+V` paste and clear selection
  - `Esc` / `` ` `` / `~` clear selection
- Song slots:
  - `Alt+B` edit slot A/B
  - `Ctrl+B` play slot A/B

## Audio Safety
- Master high-cut is currently hardcoded in DSP (`kMasterHighCutHz` in `src/dsp/miniacid_engine.h`).
- Keep defaults conservative for compact speakers.

## Documentation Rule
When changing controls/behavior, update in the same change set:
- `keys_sheet.md`
- relevant `docs/*.md` page docs
- `README.md` if architecture/workflow changed

## Build / Check
- Quick build: `./build.sh`
- Release build: `./release.sh`
