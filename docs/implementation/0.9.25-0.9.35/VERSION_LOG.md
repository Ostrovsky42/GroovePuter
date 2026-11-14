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
