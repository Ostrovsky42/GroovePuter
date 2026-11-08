# Long Melody E0–E6: three-stage review — 2026-10-22

Scope: uncommitted changes in `experiment/long-melody-viability-e0` and their integration with the existing playback, storage, diagnostic build, and hardware evidence. The review counter was reset whenever a defect was found; the three passes below were completed consecutively after the last correction.

## Defects found and corrected before the clean sequence

1. `extractBarRange()` could return `Ready` for a valid legacy buffer with duplicate onsets, while `replaceBarRange()` rejected that slice. Extraction now rejects it without mutating the destination. A RED→GREEN regression and randomized extract→replace round trips cover the handoff.
2. The runtime instrumenter replaced `SD.open(` inside a `LOG_DEBUG` string, making the failure message misleading. It now preserves that literal and rejects other ambiguous string occurrences. A source regression covers the exact log line.
3. The Scene save report described `sceneSaveMaxUs` as an entire save, although the probe stops after JSON serialization, write, and flush. The hardware report now names that window and keeps the full UI stall separately.
4. Hardware instructions pointed to an old M2 image without `sourceA/sourceB` for the upcoming musical matrix, and treated the already completed census as future work. They now point to `build/long-melody-e0-m2-source-diagnostic/GroovePuter.ino.bin` (`545955f1…`) after E0 repeatability is captured.
5. The previously installed E0 image (`2d97ac49…`) and the later corrected review build (`31e12c81…`) were easy to conflate. The hardware report assigns the older observations to the older image. After this review, `31e12c81…` was flashed; its application boot remains unverified.

## Three consecutive clean passes

| Pass | Review focus | Evidence | Result |
|---|---|---|---|
| 1 | Bounded edit semantics and real 8-bar playback | `run_long_melody_range_edit.sh`, including duplicate-onset and randomized round-trip checks; `run_pattern_phrase_p3_tests.sh`, including 4→5 gate continuity and 8→1 release/retrigger | PASS |
| 2 | Diagnostic transformation, memory, and build provenance | ASan/UBSan range test; three E0/census source suites; fresh FS1B runtime build (`31e12c81…`), fixed DRAM 190,240 B; 471 candidate FatFs map references and zero stock FatFs references | PASS |
| 3 | Undo, persistence/NEXT integration and evidence consistency | U4B5 Undo, H5 SDL process restart, M4 NEXT material; earlier P3-U1, MelodyStore, promotion, and FS2A focused runs also passed; artifact hashes, hardware status, and `git diff --check` reviewed | PASS |

Existing C++ bit-field and `snprintf` warnings appeared in the host builds; the focused commands exited zero. The pass result covers host implementation and diagnostic build consistency. It does **not** close the Cardputer E0–E6 hardware gate: the newly flashed E0 `31e12c81…` needs captured application boot, repeat cold boot, and controlled slot switching; the source-tagged M2 image still needs A Melody, dual Melody, NEXT, persistence/failure, endurance, and external MIDI cases. Upload verification after the review does not count as a boot or musical test.
