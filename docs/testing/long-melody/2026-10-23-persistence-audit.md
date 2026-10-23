# Persistence audit — 2026-10-23

Scope: all existing persistence mechanisms in the Long Melody candidate, in isolated worktree `experiment/long-melody-viability-e0`. The persistence diagnostic image `6cb56545…` was flashed on 2026-10-23 after this audit; esptool verified the written hash. Hardware persistence acceptance is still pending.

## Hardware acceptance received

The user accepted Cyber-theme Synth A/B STEPS rendering, slot switching, STEPS↔MELODY switching, an eight-bar Melody and its 8→1 loop, then the combined A+B Melody/drums/FX/NEXT/GO/STOP/PLAY and Song Pattern→Melody→Pattern run. These are user-reported audible/visual results, not captured heap/underrun traces.

Persistence failed: after reboot the user manually selected Load Project and reported bars 2–8 missing. The exact post-load length and pre-save ACCEPT toast were requested but not supplied. Do not infer that the SD payload itself was truncated: the explicit-load defect below can leave a shorter/wrong session buffer even when all eight bars remain on disk. The last-project auto-selection observation still needs a captured device boot.

## Findings, reproduction, corrections

| Finding | Evidence before correction | Correction |
|---|---|---|
| Explicit Load Project read JSON but omitted material descriptors/page restore and Melody hydration | Real engine retained the unrelated 1-bar Working Melody instead of accepted 8-bar data on both voices | Restore page-zero descriptors and full Melody payloads; reset preceding project's Working/saved identity/lineage state before applying loaded runtime settings |
| Explicit Save and autosave recovery omitted the canonical resident page file | A pre-existing page file overrode newer saved Pattern/drum JSON fields at cold boot | Explicit Save and recovery autosave now write the current canonical resident page as well as their JSON; propagate a failed page write |
| Save As copied Pattern pages but no Melody files; existing destination names skipped copying nonresident pages | Copy test lost/reused wrong 8-bar payloads, including voice B at global slot 255; overwrite scenario retained foreign page data | Copy both committed Melody generations and legacy gpml files; perform project cloning in Save As rather than name selection, for both new and existing destinations; check publication-record save result |
| Project clear left Melody files behind; named cleanup duplicated only Pattern cleanup | Both target Melody payloads still loaded after clear | Both clear entry points share page/publication/Melody cleanup; preserve material identity high-water |
| New blank project and UI Clear retained the previous project's Working Melody | Real-engine New and real ProjectPage Clear tests kept both old Melody sources | Clear session Working/saved identity/lineage buffers and destination material files, then publish an empty page-zero project; route UI Clear through the same durable core operation and report failures |
| SDL ignored failure writing the last-project pointer | Making the pointer path a directory still returned write success | Propagate pointer write failure; Cardputer already checked this result |
| Project Save success toast hid unaccepted Working data | Code review: Save serializes CANONICAL, while WORKING requires ACCEPT | Show `PROJECT SAVED; WORKING UNSAVED` if a Pattern/Melody still needs Alt+Enter |

A successful JSON decode now resets the resident page identity to zero, matching boot semantics. This prevents a previous project's selected page from labelling newly loaded material. Paging and material restore remain allocation-bounded; there is no new resident musical buffer.

## Inventory of intended durable state

| State | Persistence owner | Verification and scope |
|---|---|---|
| Pattern A/B, step notes/accent/slide/velocity/timing/probability/FX; drums and automation | `pattern_paging.cpp`, project Scene codec | Page CRC/backup/isolation/copy tests; scene/generation round trips; canonical-edit restart regression |
| Melody A/B, 1/2/4/8-bar length and full event timeline, stable material identity | `melody_store.h`, `melody_promotion.h`, page descriptors and publication record | Encode/decode/invalid data/failure paths; copy/clear; real-engine explicit reload; separate OS-process restart with A+B both 8B and one distinct event in each bar |
| Song slots, row references, length, loop/reverse, Phrase Bank and custom phrases | `scenes.cpp`, `phrase_persistence.h` | Scene round trip and Phrase persistence; Unified Song slots runtime suite |
| Synth engine and normalized patch, mute, volume, delay/distortion; drum engine/FX | Scene synchronization, streaming Scene codec | Scene/generation round trips, synth persistence source checks, full host suite |
| FEEL grid/cycle/swing/profile/texture and generator genre/key/scale parameters | Scene codec | Scene round trip, GF2-I2 FEEL persistence, full host suite |
| Track output INTERNAL/MIDI/LAYER | `output_scene_persistence.h`; Cardputer streaming SamplerScene filter chains output filter | Output ownership/persistence tests; actual Cardputer integration inspected, physical restart still pending |
| Sampler pad assignment, trim, pitch, volume, choke, reverse/loop and enabled state | Stable SampleRef conversion in Cardputer `sample_scene_persistence` filter; Scene synchronization | Sampler ownership/persistence tests and source integration; sample audio stays in the global sample library |
| UI workspace/page, theme, waveform overlay and device volume | `cardputer_ui_session.cpp` Preferences record | UI session schema/sanitization and debounce/boot source tests in full host suite; physical NVS write/restart is not simulated |
| MIDI profile/ports/channels/transport source/follow, MIDI input settings | `cardputer_midi_settings_session.cpp`, settings codec/persistence | Profile and boot-order suites, settings codec round trips/corruption tests, input settings tests; physical NVS/device routing is not certified |
| Per-file SMF output routes | `cardputer_smf_route_persistence.cpp`, route profile codec | Full-host route profile/runtime/source tests; physical NVS/device output remains separate |
| Last selected project, autosave recovery and project main/backup promotion | `scene_storage_cardputer.cpp` | Transactional file flow and return-value inspection; SDL process restart proves pointer/reload semantics, not Cardputer SD electronics; manual auto-selection report remains open |

Other filesystem writers were also inspected:

- `voice_cache.h` stores re-creatable speech RAW caches, validates written byte counts, and deletes a short write. These are caches rather than authoritative Melody/project data.
- The current Cardputer sketch leaves recorder construction and `setAudioRecorder` commented out. Its dormant recorder implementation does not propagate header/short-write failures through the interface. The desktop recorder has similar missing failure reporting. These paths have not received recording acceptance.
- `renderProjectToWav` has no production call site in this checkout. Source review found a fixed four-bar Pattern-mode duration (so it cannot export a complete eight-bar Melody cycle) and ignored sample/header write results before returning true. These are **open export findings**, not fixed or certified by the project-persistence patch. They do not establish a loss of the editable gpml payload.
- MIDI import saves through the same Pattern page owner; its existing page ownership/backup tests passed. Diagnostic census files are temporary uniquely named probes with cleanup checks from the previous M2 run.

PERFORM keyboard/ARP/Euclid/voicing configuration has no existing project/session serializer and currently resets with its runtime object. It is not silently included in the durable-state claim. Persisting those settings would be a separate feature. Held/live notes, NEXT/GO, Undo, source anchors/lineage and Tape audio buffers are session state; H5 verifies that unaccepted/queued material does not replace accepted data on restart.

## Executed software checks

All commands below exited zero after the relevant corrections:

- `bash tests/run_pattern_phrase_p3_tests.sh`: existing real-engine source/playback cases plus new `test_synth_note_display_truth`, `test_project_melody_copy`, and `test_project_melody_reload`. Log: `/tmp/gp-persistence-p3-final.log`.
- `bash tests/run_host_tests.sh`: `/tmp/gp-persistence-host-final.log`.
- Output ownership, sampler persistence ownership, Melody Store, Melody promotion, MIDI profile selection, MIDI settings boot-order suites: `/tmp/gp-persistence-run_*.log`.
- The following binaries were built against the freshly rebuilt production P3 archive and executed: `test_scene_roundtrip`, `test_generation_scene_roundtrip`, `test_gf2_i2_feel_persistence`, `test_phrase_persistence_preview`, `test_material_accept`, `test_melody_slot_navigation`, `test_unified_song_slots`, `test_pattern_paging`, `test_pattern_paging_ownership`, `test_0_9_11_c1a_material_id_persistence`. Logs: `/tmp/gp-persistence-test_<name>.log`.
- Strengthened H5 test: independent writer/reader OS processes, accepted A+B eight-bar payloads, exact event/version restore, unaccepted material excluded: `/tmp/gp-persistence-test_h5_sdl_process_restart-final.log`.
- MIDI transport settings persistence and MIDI input settings: compiled with their production codec/settings sources and executed successfully. The first attempt to link the transport codec test with only the SDL archive lacked those codec objects; it was corrected and passed.
- Cardputer diagnostic/memory source regressions, memory closure source test, unified Song display contract and `git diff --check` passed.

Reproduction of the two new persistence cases is included in the P3 runner. The first copy/clear run failed four expectations, initial explicit reload/stale-page run failed three, blank-project test failed two, the SDL pointer test failed one, and the final recovery/UI-Clear test failed three before correction. The final ProjectPage Clear test also checks that a successfully persisted clear leaves the Scene revision clean.

The unrelated old UI-P0 literal source assertion documented in the note-display report already fails on HEAD; no full-repository-all-tests claim is made here.

## Remaining hardware evidence

E5 is **RETEST PENDING on installed image `6cb56545…`; software fixes pass; the preceding image failed hardware persistence acceptance**. The actual SD/NVS restart path, last-project auto-selection, read-only/missing SD behavior, and heap/audio plateau require capture on the corrected image. Per-file backups/CRCs and return values do not constitute an atomic transaction across an entire project under power loss. No power-cut endurance test was run.

The prepared runtime adds `[PERSIST-STATE] project=… barsA=… eventsA=… dirtyA=… barsB=… eventsB=… dirtyB=…` so the next device capture can distinguish wrong project selection, unaccepted material, absent payload and a short restored Melody.

Final build results:

- SDL executable build: PASS (`make -C platform_sdl -j2 CXX=g++`), `/tmp/gp-persistence-sdl-build.log`.
- Cardputer normal CDC runtime FS1B: PASS. Candidate-only FatFs link proof passed; stock archive absent. Fixed DRAM remains **190,240 B**, under the provisional 191,488 B limit by 1,248 B; this is not a derived runtime safety margin.
- BIN: `build/long-melody-persistence-runtime/GroovePuter.ino.bin`, SHA-256 `6cb56545a2ae8331873a98c15a54de4dc34aa6c9592d3dd01cee697315789d00`.
- ELF SHA-256: `fe848becc3b60b45e27a75e709115724b736195bb0b2ff810d9124f9e8b38a60`.
- Build log: `/tmp/gp-persistence-cardputer-build.log`.
- The first diagnostic compile exposed a missing include in the new persistence status insertion; its initial include fix was overwritten by a cached instrumenter string. Both were corrected before the successful final compile. These were diagnostic transformation/build errors, not hidden passing builds.

After the audit the user requested upload. BIN `6cb56545…` was written to `/dev/ttyACM0`; upload exited zero, esptool verified the hash, and the port returned after RTS reset. A monitored save/restart/automatic project selection/explicit-load check on Cardputer is still required before E5 can pass.

The immediately following 60-second monitor (`logs/serial-60s-20261023-133855.log`) connected but received no application lines. Application boot and persistence are not verified by this window. The user was instructed to release G0 and press physical Reset if GroovePuter had not opened.

Hardware update: the user confirmed Save/Save As/Reset/Load restores both eight-bar Melody voices with notes. This is user-reported device acceptance; the later runtime monitor contains no save/reload operation. See `2026-10-23-internal-acceptance.md`.
