# Long MIDI Playback - Current Findings and Expansion Plan

## Summary
We attempted to expand pattern banks beyond A/B to support long MIDI imports. On DRAM-only devices (no PSRAM), increasing bank count causes a crash (abort) due to higher RAM usage. The device log confirms PSRAM = 0 and an abort during UI navigation shortly after extending banks.

## Current State (Stable Baseline)
- Bank count remains 2 (A/B) for RAM safety.
- Page paging prototype was removed after causing UI lag on hardware.
- MIDI import supports start pattern offset (skip ahead).

## What We Learned
- Scene data stores all banks in RAM.
- Increasing banks multiplies the size of `Scene` and causes abort on DRAM-only hardware.
- Long MIDI playback is possible only if we avoid keeping all banks resident.

## Expansion Path ("Spotlight" Paging)
Goal: Allow long MIDI playback without PSRAM by keeping only two banks (A/B) in RAM and paging the rest from SD.

### Approach
- Treat A/B as a sliding window ("page") over a larger pattern set stored on SD.
- On page change:
  1. Save current A/B to SD.
  2. Load requested page, or clear if missing.
- MIDI import targets a specific page + offset.

### UI Behavior
- Add a page indicator (e.g., P1, P2, P3).
- Add shortcuts to change pages (e.g., Fn + Left/Right or Alt + Left/Right).
- Keep `B` for A/B within the current page.

### Storage Layout (Example)
- `/patterns/303A/page_XX.bin`
- `/patterns/303B/page_XX.bin`
- `/patterns/drums/page_XX.bin`

### Risks
- SD I/O latency on page change.
- Need robust dirty tracking to avoid data loss.
- Must handle missing/corrupted SD files gracefully.

## Next Steps (If We Proceed)
1. Implement page store module with read/write and dirty flags.
2. Add page navigation to Pattern Edit + Drum Sequencer.
3. Update MIDI import to write directly to pages.
4. Add a minimal fallback when SD is missing (stay on page 1).
