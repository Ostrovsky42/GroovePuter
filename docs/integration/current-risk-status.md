# Current critical-review risk status

Updated2026-10-26 after the first matched-input sound audit; firmware unchanged.
Production source: `9e6c3ddf434267f91b4cfca2e1b9ea374f85976e`. Flashed BIN SHA-256: `349b8ea4f6a273486d3a6cb8cebb51cbfa2efe3775cfbb53e6b03f0b286277c1`.
Experimental branch: `integration/20261025-g6-dsp-ab-presets`.
Exact upload evidence: [TYPE manifest](type-popup/manifest.json), [flash record](type-popup/flash-record.json).
This is a test candidate, not an accepted standalone release candidate. App boot/functional acceptance for this latest BIN has not been captured. No risk scores are reassigned here.

| Risk | Current status | Evidence and progress | Remaining closure |
| --- | --- | --- | --- |
| R1 Product focus | POLICY SET / AUDIT OPEN | CORE/SUPPORT/FROZEN policy ratified; current edits improve existing paths. DUAL OSC integration was explicitly requested by the user as experimental work, enabled by its existing build flag. | Record this as a scoped experimental freeze exception, not part of accepted CORE/release scope; audit three consecutive PRs and README/Quickstart alignment. |
| R2 Musical quality | G2 NOT VERIFIED | Techno/Dub-Reggae genre/seed preregistration exists in the prior acceptance dossier. No new blind qualification in this session. | All 16 fixed sequences, canonical structural diffs, normalized audio and three blind ratings, ≥6/8 per genre. |
| R3 DSP preview | G4 OPEN / USER-REPORTED ISSUE | DSP-A release and ASan/UBSan, original six-engine patch/level roundtrips pass. User reports quiet TB303 Acid. Fresh156-case matched-input comparison found Classic Acid+0.0043 dB, unchanged Acid-filter held notes, and a separate14.56 dB DUAL OSC/TB Classic level gap for C3. Actual quiet hardware patch/context is still needed; no DSP/gain change applied. | Reproduce/compare matched patch, notes, velocity and levels before DSP-A; then speaker/jack, blind A/B, 10-minute audio, NoteOff/switch and underrun qualification. Do not mask with global gain. |
| R4 UX | LOCAL FIXES VERIFIED / G1,G7 OPEN | Automated real-UI tests verify TYPE engine list, separate presets, both voices, cancel/confirm and stepping. Previous preset popup had SDL screenshots. Interactive TYPE-modal screenshot was not completed. | Three timed cold starts for G1; three independent users for G7 with no developer coaching. Prior persistence cold cycles do not establish G1 timings. |
| R5 Definition of Done | RELEASE STOP | Exact current source and flashed BIN are recorded. The latest combined BIN has no completed six-gate/runtime acceptance. G5 remains DEFERRED. | Select/freeze final RC; all G1/G2/G3/G4/G6/G7 and runtime evidence on that candidate, independent sign-off and limitations. |
| R6 Cardputer constraints | STATIC DRAM PASS / RUNTIME OPEN | Latest ADV build: .data41,996 B, .bss142,256 B, total184,252 B, limit191,488 B unchanged. Previous short device captures showed 0 underruns under uncontrolled loads. | Paired #495/current CPU, heap/largest, stack and workload logs, 30-minute stress. No old heap/underrun result transfers to latest image. |
| R7 Ownership complexity | OWNER MAP DRAFT / AUDIT OPEN | Existing owner map and local RED→GREEN storage/UI repairs exist. TYPE uses the existing modal and audio-guard/engine-switch path. | Update/archive owner map against integration source; independent boundary review plus three bounded bugfix audits. No claim all ownership risk is closed. |
| R8 Persistence trust | POSITIVE HARDWARE PROGRESS / G6 PARTIAL | Real host fault tests cover refused/corrupt Load, bounded reads, prepared material, names/pointer preservation and New/Clear. User reported accepted A+B eight-bar restoration, Clear/Reset/Reboot/Load and three cold cycles on prior New/Clear BIN22679b20… . | Current-BIN regression, canonical event/Song/HGS/patch hashes, negative SD cases and exact reported Save-refusal boundary. Earlier missing test-save payload/recovery remains unresolved; initial 2–8-bar loss report was corrected by the user as omitted ACCEPT. Power-cut atomicity UNVERIFIED. |
| R9 AI governance | LOCAL PROVENANCE PRESENT / THREE-PR AUDIT OPEN | Isolated worktrees, preserved merge history, exact code/BIN manifests, regression logs; user explicitly authorized local integration and firmware uploads. No remote push or release-branch/PR merge in this integration. | Three consecutive PR evidence/scope audits and independent review. Current acceptance matrix, operator sheet, owner map, ratified policy and sealed seed manifest are now consolidated in this integration worktree. Older worktree matrices remain historical; independent three-PR audits are still needed. |

## Release/gate interpretation

None of the nine risks has its full exit criteria closed. This does not mean software repairs or the user's positive hardware cycles are worthless; it means the formal target scores cannot yet be supported. The latest BIN has no complete formal G-gate PASS. G5 REAPER stays DEFERRED. Cold cycles on the older image support R8 progress but are not current-BIN G6, canonical-hash evidence, or G1 timing evidence.

## Next sequence

1. Investigate the quiet TB303 Acid symptom, compare the exact old/new patch and measured level, implement only a reproduced defect fix if required.
2. Freeze one candidate image; run a combined G1/G3/G6 device session and paired 30-minute runtime protocol, including negative SD handling and data hashes.
3. Complete G2 blind musical qualification, G4 listening and G7 independent users, then independent release evidence review.

No new score is assigned automatically. No push, release merge or firmware change is performed by this status audit.

Evidence: [integration report](g6-dsp-ab-presets/README.md), [New/Clear hardware follow-up](../acceptance/critical-review/g6-new-clear-reset/hardware-follow-up.md), [TYPE tests/builds](type-popup/README.md). The current [acceptance matrix](../acceptance/critical-review/acceptance-matrix.md) supersedes obsolete installed-image statements; historical results remain archived. Fresh sound evidence is [here](../acceptance/critical-review/tb303-level-audit/README.md).
