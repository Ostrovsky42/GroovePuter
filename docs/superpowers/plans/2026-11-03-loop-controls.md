# LOOP Controls Implementation Plan

> Implement inline with superpowers:executing-plans; tests before changes.

Goal: HOLD protects the original phrase, EVOLVE selects 0..3 variation, DUET shortens dense calls.
Spec: ../specs/2026-11-03-loop-hold-evolve-duet.md.
Constraints: fixed capacity 16, no heap on musical path, DRAM <=191488, session settings.

- [x] Test HOLD with 1/2-bar capture, no overwrite, release only full future window, reset on OFF/BARS/epoch.
- [x] Implement hold state and capture suppression in call_response_listener.h; preserve original loop clock.
- [x] Test EVOLVE=0 exact plan, 1 stable rhythm, 2 policy, 3 bounded tail shifts; dense DUET shortened by default.
- [x] Implement pure planning helper and route both firmware and listener tests through it.
- [x] Add HOLD and EVOLVE UI rows, disabled outside LOOP; update help.
- [x] Run full Smart Play, UI regression, SMF cleanup; build CDC/MIDI-only, check DRAM.
- [x] Record actual verification and hardware limitations; no claim of listening acceptance without listening.

Verification (2026-11-03): full Smart Play suite PASS; PERFORM UI source regressions PASS; SMF cleanup PASS; focused independent review found two defects, both reproduced and fixed. Final CDC build: flash 1669610, DRAM 185412/191488. Final MIDI-only: flash 1718274, DRAM 185852/191488. Hardware listening and visual UI validation not performed for these controls; no new image uploaded in this turn.
