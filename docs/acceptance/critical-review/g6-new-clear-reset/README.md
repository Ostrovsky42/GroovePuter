# G6 New/Clear leaves dangling Melody descriptors

Status: reproduced RED; narrow local repair and full G6 host suite GREEN. SDL and Cardputer ADV builds PASS. Candidate uploaded and app data hash verified after explicit user request; 60-second post-flash monitor connected but captured no application output; app boot remains unconfirmed. Functional hardware verification remains OPEN.

## Device evidence

The requested 180-second monitor captured the user's report `Failed Save project`. The serial instrumentation does not identify the exact refused Save boundary, so that symptom remains OPEN. It does show two successful transactional JSON writes to `/scenes/test-save1.json`, followed by Load refusals for missing Synth A voice0/slot0 Melody payloads. The user's message and the successful JSON writes must not be conflated into a diagnosed single Save attempt.

31 PERF samples: audio 6.1–37.8%, recorded cumulative peak 62.1%, underruns 0, free internal heap 16,356–19,120 B, largest block 7,668–8,180 B. This is uncontrolled activity, not the 30-minute gate. USB MIDI endpoint rejects/demotion persisted; G5 remains DEFERRED.

`AUTOSAVE` recovery revisions 32/41/43/45 and the explicit JSON writes prove the application wrote during this session. Earlier language that the running board's SD was untouched was too broad: the host serial monitor is read-only, but application autosave and UI actions can write. Earlier recovery-candidate records are historical observations, not proof those files still exist after this session.

## Reproduction and root cause

The real MiniAcid/SDL storage test starts with accepted A+B 8-bar Melodies at selected/nonselected slots 0 and 3 and confirms the source reloads. `createNewSceneWithName` removes the target project's payload files, resets working state, then calls `SceneManager::wipeToZero`. That function cleared step notes and songs but retained material kinds and identities. The newly saved blank page therefore still referenced four removed Melody payloads and could not reload.

Before repair, both New (another project name) and Clear (same project name) returned `created=1 stale=4 reload=0`, exit 1. The repair resets existing material descriptors in `wipeToZero`, without new storage, allocation or runtime ownership. The regression now verifies all slots are free, blank project reloads, no working Melody remains, and New preserves every source-project asset byte for byte and leaves the source loadable. Both cases pass; full `tests/run_g6_failed_load_tests.sh` exits 0.

This repair does not reconstruct absent payloads or establish the cause of the user's separately reported Save refusal. Physical New/Clear/Save/Reset/Load and SD-failure acceptance remain OPEN. No push/merge. Firmware was subsequently flashed at the user’s explicit request; see manifest/upload.log.

## Diagnostic limitation discovered during source review

The current read-only same-project directory census uses `leaf.compare(0, 5, "v0_g")` / `v1_g`, although these prefixes have length 4. It therefore skips the ordinary filename candidates. The cross-project selected-slot census is independent and produced the observed slot-0 candidate. Earlier claims that the current-project directory was exhaustively searched are not supported. The diagnostic predicate has not been changed in this New/Clear repair; correct it before relying on another recovery census.

## Candidate artifact

Base HEAD `0cfb94c38a12e42e9c8cd8f8da4c77620ed421b6` plus the uncommitted `repair.patch`. Candidate BIN SHA-256 `22679b20166e33c3b48dad0a8eb369c2a32adfc0764b36729e746f4c652d1f8a`; size 1609904 B; reported flash 1,609,742 B (+36 B from the flashed diagnostic), `.data` 41,996 B, `.bss` 142,256 B, static DRAM 184,252 B (unchanged, limit 191,488 B). No runtime allocation/state added. G6 host suite, SDL build, ADV build PASS. Runtime heap/largest and hardware New/Clear/Save/Load for this candidate remain NOT TESTED. Exact logs and patch are stored here.

After the user pressed Reset, a 60-second capture (`serial-60s-20261025-211800.log`) confirmed running app KEY/PERF/MIDI/UI telemetry on the uploaded image. Early setup text was not captured. No Save/Load marker was present in this window, so New/Clear persistence acceptance remains OPEN. Metrics are in manifest.json.
