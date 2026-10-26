# Standalone acceptance — current integration candidate

Updated 2026-10-26. Policy: [ratified closure plan](../../product/CRITICAL_REVIEW_CLOSURE_PLAN.md).

Installed test candidate: production `9e6c3ddf434267f91b4cfca2e1b9ea374f85976e`, BIN `349b8ea4f6a273486d3a6cb8cebb51cbfa2efe3775cfbb53e6b03f0b286277c1`. This is not yet the final frozen RC. [Upload provenance](../../integration/type-popup/flash-record.json), [current risk audit](../../integration/current-risk-status.md).

| Gate | Evidence already retained | Current candidate remaining | Status |
| --- | --- | --- | --- |
| G1 start | Earlier user audible playback, historical boots | Three independently timed cold starts, all tracks ≤180s | OPEN |
| G2 music | Two genres/eight seeds preregistered | Sixteen native-workflow sequences, structural/event dumps, three blind ratings | OPEN |
| G3 edit/Undo | Existing host regression coverage | Three device cases with canonical/Song hashes: Melody edit, FORM undo, refused/cancelled DEVELOP | OPEN |
| G4 sound | DSP-A host/sanitizer evidence; no dry Classic Acid level regression in fresh matched-input renders | Resolve actual hardware quiet TB303 context; speaker/jack and blind A/B, release/switch/underruns | OPEN |
| G6 persistence | Host failure paths/New/Clear/patch roundtrips; user three cold cycles and accepted A+B8B on earlier New/Clear image | Current-candidate hashes, three cycles, unaccepted edit exclusion, negative SD cases; safe backup route needed without card reader | PARTIAL / OPEN |
| G7 independent UX | Local TYPE/preset functional tests; older scoped SDL screenshots | Three unfamiliar users without developer coaching, timed complete workflow | OPEN |
| Runtime | Static DRAM184252 B, limit191488 unchanged; historical short logs | Paired #495/candidate CPU/heap/largest/stack, representative30-minute workload | OPEN |
| G5 REAPER | Explicitly deferred | No work for this standalone goal | DEFERRED |
| Independent release review | Local evidence/provenance collected | One final SHA/BIN, complete primary evidence, reviewer sign-off | OPEN |

Release decision: STOP. No current candidate gate has a complete primary-evidence PASS. Historical positive checks remain useful regression evidence; do not discard them or label current G6 data loss based on the corrected omitted-ACCEPT report.

Next device actions are combined in [W1 session](w1-operator-session.md). [Fresh TB303 comparison](tb303-level-audit/README.md) narrows the sound investigation; it does not certify hardware listening. [Owner map](owner-map.md) and R1/R7/R9 three-PR audits remain review work.
