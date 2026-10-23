# Synth NOTES display review — 2026-10-23

Scope: Synth A/B NOTES in `experiment/long-melody-viability-e0`, including the existing dirty Long Melody candidate. This candidate was flashed on 2026-10-23; esptool verified the written hash.

## Findings and corrections

1. RETRO/AMBER rendered the Pattern selection cursor instead of the actual current Pattern. A rejected switch or an activation without a cursor update could therefore draw another slot. All three styles now use `MiniAcid::synthPatternForDisplay`: audible quantized snapshot, exact-bound Working Pattern, then active accepted Pattern (empty for an unassigned Song row).
2. The Synth container's Melody branch bypassed the Pattern child's Song WAIT/FAIL display. It could show retained notes while Awaiting deliberately silenced playback. Awaiting/load-failed states now use the WAIT/FAIL presentation before reading retained Melody data.
3. MINIMAL drew a playing-step marker at step zero during STOP because `currentStep()` returns zero there. The marker now requires running transport.
4. After switching a held Song Melody back to STEPS, the derived display state and address still reported the held Melody. HOLD identity now applies only while the active material is Melody; STEPS shows the actual Song Pattern.

The display accessor is read-only and allocates no state. Playback, DSP, mute, probability, and output routing semantics are unchanged. Visible sequence notes remain editable at STOP or MUTE; presence of a note is not an assertion that a physical output produced sound. The shell's existing transport/mute indicators remain relevant.

## Verification

`tests/test_synth_note_display_truth.cpp` drives real MiniAcid and real pages through a recording IGfx, for both voices and MINIMAL/RETRO/AMBER. It checks:

| Scenario | Result |
|---|---|
| Inactive cursor cannot replace current grid | PASS |
| Working notes take precedence over accepted notes | PASS |
| Audible quantized snapshot takes precedence until activation | PASS |
| External slot activation visible on the next draw, without tick | PASS |
| Empty Song row has no resident-slot phantom notes | PASS |
| Awaiting and failed Song load show WAIT/FAIL, not retained notes | PASS |
| Held active Melody remains visible | PASS |
| Held Melody → STEPS updates grid and displayed address | PASS |
| STOP has no MINIMAL playing-step highlight | PASS |

The initial reproduction reported 44 failed expectations. A later added Held→STEPS reproduction reported 14 failed expectations before its correction. The final test reports zero failures and is included in `tests/run_pattern_phrase_p3_tests.sh`.

- P3 real-engine bounded source, per-synth cardinality, phrase-relative playback, and new display test: PASS. Log: `/tmp/gp-note-display-p3-final.log`.
- Unified Song slots real-engine suite (including HOLD, WAIT, NEXT/GO, load failure, persistence): PASS, linked to the same freshly rebuilt production archive. Log: `/tmp/gp-note-display-song.log`.
- Working UI note-source, P3-U1 source projection, and unified Song display source contracts: PASS. Updated two source assertions to the shared display accessor.
- Identity-bound Working Pattern edit and guarded manual retarget real-engine tests: PASS, linked to the rebuilt archive. Logs: `/tmp/gp-note-display-test_0_9_11_m_working_manual_pattern_edit.log` and `/tmp/gp-note-display-test_0_9_11_m_working_manual_retarget.log`.
- SDL executable build: PASS (`make -C platform_sdl -j2 CXX=g++`). Log: `/tmp/gp-note-display-sdl-build.log`.
- `git diff --check`: PASS.
- The older `test_0_9_9_ui_p0_source_regressions.py` fails at its literal `setCurrentSynthPatternIndex(0/1, pat)` assertion. The same assertion fails when its engine input is replaced with `git show HEAD:src/dsp/miniacid_engine.cpp`; the failure predates this patch. It was not changed to hide that result.

Recording-Gfx checks exercise render commands rather than framebuffer pixels or physical sound. Hardware acceptance remains pending.

## Prepared Cardputer image

- Path: `build/long-melody-note-display-runtime/GroovePuter.ino.bin`.
- Runtime instrumentation and normal CDC profile, PSRAM disabled, FS1B dynamic FatFs.
- Candidate-only FatFs link proof: PASS; stock archive absent.
- Fixed DRAM: 190,240 B, same as the previously flashed candidate; provisional budget 191,488 B.
- BIN SHA-256: `b93f21f1ba22f5196d5b0fb1889fa20e56ca880f298842deb7ab31edaf5485f7`.
- ELF SHA-256: `31ae70ac2ebc299ef37f2d780da55db790e2835161b858b45762d71b3f65076c`.
- Build log: `/tmp/gp-note-display-cardputer-build.log`.

The device now has this candidate (`b93f21f1…`). The upload exited zero, esptool verified the written hash, and the serial port returned after RTS reset. Check visible notes against playback during working edits, rejected slot changes, Song Pattern/Melody transitions, WAIT/load failure, Alt+R after HOLD, and STOP. External output and internal sound need separate observation.

Post-upload Serial window `logs/serial-60s-20261023-115307.log` connected but received no application lines. This verifies neither application boot nor a boot failure. User was instructed to release G0 and press physical Reset if the application had not opened; hardware display acceptance remains pending.
