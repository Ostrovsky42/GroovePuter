# Performance / HGS boundary review — 2026-10-14

Reviewed checkout: `/home/gg/.codex/worktrees/hgs-review-5e98`.
HEAD: `5e98acbddd534a1d22595a0a68b36efe31ceae50`, including its uncommitted Smart Play fixes and diagnostic/startup changes. Latest flashed diagnostic ELF SHA-256: `dc66389b6fdee6a1ab31c5b9b33a2313588ea55e93eb630ca11257cb70e086ac`.

Verdict: listening acceptance is not established. Model tests pass; two additional boundary defects reproduce in review probes. Review does not certify all production paths or physical MIDI/audio timing. Production source was not changed during this review.

## Findings, ordered by priority

### R1 — P1: Harmony event identity is lost at the performer boundary

`src/input/performance_keyboard.cpp:904` copies only `snapshot.key.eventOrdinal` into `PerformanceContext`. `setPerformanceContext()` at lines 789–799 compares the ordinal and pitch/context fields, but has no materialId, publicationGeneration or transportEpoch. `sameHarmonyEvent()` in `harmonic_snapshot.h:8` defines identity using all four fields.

A new material with the same chord and ordinal is therefore treated as the same event. RESTART continues its old figure; HOLD/FOLLOW also miss this source transition. The earlier uint8 ordinal saturation fix does not close this gap.

Reproduction: a copy of the passing snapshot RESTART trace replaces ordinal 256→272 with materialId 1→2 while retaining ordinal 256. Expected third note is 60; the assertion fails (exit 134). Probe: `2026-10-14-boundary-review/repro_snapshot_material_identity.cpp`.

Required correction: retain the full bounded event key and compare it at publication; cover materialId, publicationGeneration and transportEpoch changes independently, including repeated publication of the identical key.

### R2 — P2: Diagnostic Fn+D is dispatched twice

The new diagnostic HID branch in `GroovePuter.ino:1115–1119` does not set `dispatchedLetterMask`. The word branch at 1165–1189 consequently accepts the same D again: its modifier suppression handles Ctrl/Alt, but not Fn. The installed M5Cardputer `Keyboard.cpp:173–188` produces both HID and word for an ordinary letter with Fn.

The same physical press toggles the overlay ON and OFF, which can appear as a flash or an unresponsive shortcut. This defect was introduced by the latest diagnostic shortcut change.

A review probe using the actual input-edge helpers and the current branch conditions prints `Fn+D dispatch count=2`, then fails the single-dispatch assertion (exit 134). Probe: `2026-10-14-boundary-review/repro_fn_d_duplicate.cpp`.

Required correction: register this HID letter in the same deduplication mask used for plain letters; verify exactly one toggle per physical press, including modifier release.

### R3 — P2: GRAPH refusal hides the global scale and recovery action

`genre_page.cpp:532–544` checks `currentScene().generatorParams.scale` and shows only `LEGACY / N/A:SCALE` or `N/A:GENRE`. Every generated harmonic policy has scaleMask 0x2f or 0x7f; none accepts scale IDs 7, 8 or 9 (major pentatonic, minor pentatonic, chromatic). Changing genres with one of these global scales selected can therefore leave every genre unavailable.

The similarly named PERFORM SCALE edits `PerformanceKeyboard::cycleScale()` (`perform_page.cpp:221`), a different owner. Changing that live-input scale does not unblock the generation scale. GENRE does not expose the actual scale causing refusal or a recovery action.

The claim that GRAPH is unavailable everywhere was not reproduced under supported global scales. Direct capability probe for Acid/Rolling (mode 0, recipe 7): Minor/Major/Dorian/Phrygian/Mixolydian return Ready; Lydian/Locrian/both pentatonics/Chromatic return UnsupportedScale. The production UI route additionally accepts this Acid/Rolling recipe through `selectStrongRhythmRoute()`.

Required correction: show the project scale beside HARMONY, distinguish it from PERFORM scale, and provide a reachable route to change it. Keep explicit genre prohibitions; do not silently expand masks to make the UI green. Add a real GenrePage interaction test using the project scale and supported recipe.

### R4 — P2: Ctrl+A has incompatible context meanings without immediate feedback

`pattern_edit_page.cpp:368` maps Ctrl+A or Alt+A to step accent. `tb303_params_page.cpp:965–969` maps Ctrl+A to cutoff=800 Hz and returns without a toast identifying the reset. Existing source tests intentionally preserve this reset shortcut. Thus the same shortcut can change the filter while the user expects accent.

The code does not play a UI beep in either handler. The reported squeal is not acoustically reproduced here. Cutoff alone does not determine the sound: `mini_tb303.cpp:290–313` applies envelope modulation and profile-dependent resonance; Ctrl+A retains resonance and all other patch parameters.

Required correction: explicit context/action feedback and consistent musician-facing help. Reproduce the squeal with the affected engine, filter profile and resonance before changing cutoff defaults or filter DSP. No evidence currently supports declaring 800 Hz itself a numeric DSP defect.

### R5 — P1 acceptance gate: runtime contiguous-memory failure is not covered by the build budget

Hardware supplied `[DISPLAY-BUFFER] ... bytes=64800 free=125724 largest=63476`, followed by vector allocation abort. `cardputer_display.cpp:327` still performs a throwing contiguous allocation without a controlled failure route. A static DRAM total of 189,636 bytes does not prove a 64,800-byte contiguous block is available after peripheral setup.

The uncommitted startup change now allocates framebuffer before I2S. This is a plausible allocation-order mitigation. The earlier statement that I2S was definitively the cause exceeded the available measurements: only the post-I2S block was measured. The available evidence does not include a complete after-display/after-I2S/after-engine heap and task-stack acceptance log for the current image.

Required gate: record successful display/I2S/audio/UI initialization and free/largest/high-water values on the actual board; handle allocation failure explicitly; verify repeated generation/save/load/USB reconnect. Do not solve this by lowering audio stack/DMA budgets without measurements.

### R6 — P2 listening gate: harmonic event traces do not validate the audible Synth B representation

`miniacid_engine.cpp:4236–4263` maps a chord-voice batch to its highest currently active note on the internal monophonic synth. Polyphonic chord notes are emitted separately on MIDI. `mini_tb303.cpp:57–61` uses a sustain level of 0.78 until gate release. A long or overlapping chord plan can therefore become an almost continuous single sustained line on the internal B voice.

This can explain a dense hum without proving a stuck-note bug. The HGS idiom consumption test checks pitches and preserved step flags/timing; it does not render audio or quantify gate coverage, rests, release tails or effective internal chord articulation. Without the exact genre/variant/engine and observed STOP behavior, the user's case cannot be classified as intended pad behavior or a release defect.

Required gate: compare generated B event traces and rendered audio for affected genres; measure occupied-time ratio, note-off/rest windows and release behavior after STOP. Require genre-specific musical acceptance rather than one universal density clamp.

## Boundaries checked without a new confirmed finding

- Shared harmonic quality/scale registry and explicit legacy projection: unsupported qualities fail explicitly in the reviewed patch. Existing registry/projection tests pass.
- Pure pitch-set, PCVL, relation, genre policy, deterministic route and anti-repeat code: HGS suite passes. This is invariant evidence, not a musical-quality verdict.
- Song harmonic codec/storage path inspected: version/bounds/CRC coverage exists; storage uses temporary write and readback. Publication revalidates targets after sidecar I/O. No new persistence defect established in this targeted review.
- MIDI source/session/cable ownership and capacity fixes: full Smart Play runner passes, including ingress/queues/ownership/external performer tests. Physical disconnect/flood/latency remains unverified here.
- Audio ownership: diagnostic display changes execute on the control/UI side; no new sample-loop harmonic interpretation was found in reviewed paths. This is not a complete concurrent-access audit.

## Verification executed

- `bash tests/run_hgs_tests.sh`: PASS, exit 0, all configured groups.
- `bash tests/run_smart_play_tests.sh`: PASS, exit 0, all configured groups.
- Separate production graph capability probe: Acid/Rolling is Ready on global scale IDs 0,1,2,3,5; UnsupportedScale on 4,6,7,8,9.
- New material identity RESTART probe: FAIL, exit 134.
- Fn+D single-dispatch probe: FAIL, exit 134 (two dispatches).
- No new firmware build/flash during this review. No physical audio capture or listening test performed.

The existing listening package already records hardware/listening NOT RUN and RETURN rejection of 4,643/8,704 attempts (53.3%). That reported route limitation is not newly reproduced by this review and remains a separate musician acceptance issue.

## Order before listening acceptance

1. Fix R1 and R2 with behavioral regressions.
2. Close R3 with the real project-scale UI workflow; verify supported Graph selection and a generated Take on the device.
3. Close the startup memory/audio gate in R5 with actual measurements.
4. Make Ctrl+A action visible (R4), reproduce the affected filter sound, and distinguish dense B material from ownership/release defects (R6).
5. Run focused listening comparisons for supported genres/variants, including STOP, transitions, GRAPH/LEGACY and RETURN rejection. Record observations before declaring readiness.
