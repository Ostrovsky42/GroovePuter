# Milestone log

## Version 0.9.25 — Foundation Audit & Contract Freeze

**Version:** 0.9.25  
**Base commit:** `28918d9b78e9a527ce2b5ce201a465af5126f76a`  
**Result commit:** `793fdbcf110a85ecf73fdfbf2dac7818b3128652`  
**Goal:** Verify and record the actual `feature/0.9.24` baseline, freeze existing ownership/storage contracts, and establish repeatable host, SDL and firmware evidence.  
**Implemented:** Baseline, architecture, MIDI, resource, test and user journey audit documents; SDL screenshots; corrected stale 0.9.24 UK Garage press-alternation claims. No production behavior changed.  
**Not implemented:** 0.9.26 routing/lifecycle additions and all later roadmap features. Live MIDI route rebinding remains deferred.  
**Tests added:** None. Existing test suite and scripts were reused; only test comments were corrected to match verified H2 behavior.  
**Tests passed:** Full local `bash tests/run_host_tests.sh` with `TMPDIR=build/tmp-host-suite` (exit 0); local SDL build and limited keyboard-driven UI scenario; exact-baseline local FS1B firmware build, link-map proof and DRAM gate; `git diff --check`; exact-head remote Cardputer ADV, MIDI Targets and Memory workflows.  
**Tests failed / pending:** Two earlier local host attempts failed (runner shell parse error during a concurrent script edit; later transient `ENOSPC` compiling an SDL-linked target); the stable rerun passed. The separate remote Core workflow host job remained `in_progress` at the latest check.  
**Memory delta:** 0 B source delta from baseline. Local fixed global DRAM 187,612 B / 191,488 B; 3,876 B headroom, same as exact-head CI.  
**Firmware size delta:** No firmware code change. Local FS1B program-storage report 1,742,066 B; exact-head CI reported 1,742,082 B (16 B difference). Local app-only BIN 1,742,224 B.  
**Known risks:** Provisional DRAM exception with only 3,876 B headroom; unfinished release-profile build fingerprint; route lifecycle test gaps; user-facing documentation is stale; no physical audio/MIDI acceptance.  
**Hardware status:** `NOT TESTED`  
**Decision:** `GO`

## Version 0.9.26 — MIDI Routing & Note Safety

**Version:** 0.9.26
**Base commit:** `ef5b19a96ed665c8ca33a984b86c3aad9a011624`
**Result commit:** `197ff2a8a92cecc886f019a9dc206dcd59c446b8`
**Goal:** Prevent locally forgotten USB MIDI notes from remaining stuck at a receiver after the whole MIDI transport disconnects and reconnects.
**Implemented:** On the overall mounted-to-unmounted transition, capture the active ownership table's channel mask before clearing local note state. On reconnect, send the existing bounded CC123 channel cleanup for those channels. Added a fake transport regression covering a PatternPlayer note on channel 7 and proving unrelated channels receive no cleanup. Runtime route snapshots remain frozen after startup; profile changes do not silently rebind active lanes.
**Not implemented:** Per-endpoint Tee loss/rejoin traces, integrated dispatcher overflow traces, and a live route-rebind transaction remain open. This change covers overall transport loss; it does not claim endpoint-specific USB cleanup when DIN keeps the Tee transport mounted.
**Tests added:** `tests/test_usb_midi_disconnect_cleanup.cpp`, registered in `tests/run_host_tests.sh`. The regression failed before the production fix and passed after it.
**Tests passed:** Targeted disconnect regression; complete `bash tests/run_host_tests.sh` (exit 0); Cardputer ADV FS1B build with `--warnings all`; FS1B map proof; `git diff --check`.
**Tests failed:** None in the final run.
**Memory delta:** Global DRAM 0 B versus baseline: 187,612 B / 191,488 B, 3,876 B headroom.
**Firmware size delta:** Arduino program report +64 B (1,742,130 B versus 1,742,066 B baseline); app-only BIN +64 B (1,742,288 B versus 1,742,224 B baseline). App BIN SHA-256: `9bc8336e836456b9a7cb24c2c1a08ec9f515ecb5e911859d7e048d4bc0fb26cb`.
**Known risks:** Recovery uses channel-level CC123 because the disconnected receiver's note state is unknown; another sender sharing that channel may also be silenced. Per-endpoint Tee loss/rejoin remains unaudited. Host and firmware build evidence do not establish device behavior.
**Hardware status:** `NOT TESTED`
**Decision:** `GO`

## Version 0.9.27 — MIDI Device Profiles

**Version:** 0.9.27

**Base commit:** `a4bd5dc737ad54de40f8477034cdfe498c43d393`

**Result commit:** `545a749b41cb002b12f4d806858fce119281c207`

**Goal:** Make external MIDI enablement selectable independently from the external device profile, while keeping internal audio available.

**Implemented:** Added a Project → MIDI `External MIDI` setting with `INTERNAL` and `INT + MIDI` modes. It composes with the existing persisted settings without changing schema v2; both profile and output mode are next-boot settings. The output master gate is applied to the shared `UsbMidiOutput` owner and outbound transport lifecycle events, covering generated notes, Performance, SMF and clock paths. MIDI profile selection does not rewrite musical material. Added help text and SDL evidence for preview/save and profile independence.

**Not implemented:** Runtime route rebinding, immediate output-mode changes without reboot, and physical MIDI endpoint validation.

**Tests added:** Output-mode label and next-boot settings unit tests; output-destination source regressions; registered in `tests/run_host_tests.sh`.

**Tests passed:** Targeted output-destination runner; sequential MIDI device-profile/UI aggregate; SDL build; SDL keyboard scenario covering output-mode preview/save and profile independence; Cardputer ADV FS1B firmware build with `--warnings all` and link proof. Full `bash tests/run_host_tests.sh` ran through its final `test_ui_view_continuity_loop_tab: OK` marker; the terminal wrapper closed before its numeric exit code was captured.

**Tests failed / pending:** No failure marker appeared in the full host log. The wrapper's exact exit code was not preserved, so this evidence is recorded as completion through the final test marker rather than an asserted numeric exit code. SDL used dummy audio; no audio-quality claim. No hardware tests were run.

**Memory delta:** 0 B global DRAM versus baseline: 187,612 B / 191,488 B; 3,876 B headroom.

**Firmware size delta:** Arduino program-storage report +1,056 B versus baseline (1,743,122 B); app-only BIN +1,056 B (1,743,280 B). BIN SHA-256: `223c8858cd36c390a66e9bb03a40d3c6d08be4e61cd66552c4c784a84fd37daa`.

**Known risks:** Mode is applied at next boot because the MIDI route snapshot is frozen at startup. Host mocks and firmware compilation do not prove Cardputer USB behavior or sound. The full-suite terminal wrapper's exit code was not retained.

**Hardware status:** `NOT TESTED`

**Decision:** `GO`
