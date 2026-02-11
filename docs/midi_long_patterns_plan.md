# Long MIDI Playback [COMPLETED]

## Summary
Seamless long **playback** across many pages is now fully implemented (see [LONG_SONG_ARCHITECTURE.md](LONG_SONG_ARCHITECTURE.md)).

## Implemented (Current Build)
- Bank count stays at 2 (A/B) for RAM safety.
- SD page save/load exists (`SceneManager::setPage/saveCurrentPage/loadCurrentPage`).
- MIDI import writes across pages (up to 128 patterns).
- Import UI supports:
  - `Start Pat` (1..128)
  - source slice (`From Bar`, `Length`)
  - `LOUD/CLEAN` import profiles
  - OMNI fallback with user notification
- Import-side crashes/log spam were addressed.

## Still Broken for Long Song Playback
- Song rows currently reference combined pattern ids in the local A/B window semantics.
- During song playback, pattern selection still resolves against current page-local banks.
- Result: importing >16 patterns works, but playback does not yet transparently walk pages as one continuous long song.

## Root Constraint
- Seamless long playback requires page switching while the sequencer advances song rows.
- Naively calling SD page load directly in real-time audio flow is risky (latency/underruns).

## Proposed Solution
1. Introduce global pattern id mapping for song mode:
   - `globalPattern = rowPattern` (0..127)
   - `page = globalPattern / 16`
   - `localCombined = globalPattern % 16`
2. Add non-blocking page-switch handshake:
   - Sequencer requests page change at safe boundary.
   - Control/UI thread performs `setPage(page)` outside hard real-time region.
   - Sequencer resumes with resolved local bank/slot.
3. Keep old behavior fallback:
   - If page switch is unavailable/failed, hold current page and report warning.
4. Optional prefetch:
   - Preload next page near end of current row to reduce gap risk.

## Short-Term User Guidance
- For reliable live playback right now, import in slices that fit active page workflow (16-pattern windows) or manually set page before the segment.
- Use `LOUD/CLEAN` mode per file for better tone/noise tradeoff.

## Next Steps (Engineering)
1. Add global-pattern song mapping in song playback path.
2. Implement deferred page switch (non-audio-thread SD I/O).
3. Add diagnostics: page-switch latency + underrun counters during long song test.
4. Add "Long Song" integration test scenario (`>=64` patterns).
