# Standalone acceptance — current integration candidate

Updated 2026-10-26. Policy: [ratified closure plan](../../product/CRITICAL_REVIEW_CLOSURE_PLAN.md).

Latest installed image: source `9e6c3ddf434267f91b4cfca2e1b9ea374f85976e`, BIN SHA-256 `349b8ea4f6a273486d3a6cb8cebb51cbfa2efe3775cfbb53e6b03f0b286277c1`. Latest local source commit `b6c3daf9fa7a59abadbe3f0b259bea350f809675` has an additional Save As rollback fix; its ADV BIN SHA-256 is `77b634b589c20cc5865eaf667134d58de36b05fd31c5180cb0cc3ed393f724f9`, built with unchanged DRAM 184252/191488 B, but NOT FLASHED. This report was committed and pushed with acceptance checkpoint `e66d36dfa37473d57141641922a29f5912ec836c`; no image is an accepted release candidate. [Installed-image upload provenance](../../integration/type-popup/flash-record.json), [current risk audit](../../integration/current-risk-status.md).

| Gate | Evidence already retained | Current candidate remaining | Status |
| --- | --- | --- | --- |
| G1 start | Earlier user audible playback, historical boots | Three independently timed cold starts, all tracks ≤180s | OPEN |
| G2 music | Two genres/eight seeds preregistered; [host workflow audit](g2-workflow-audit/README.md): Techno 8/8 layouts but one unique event set; Dub/Reggae 0/8 (NotAdmitted) | Resolve project-seed/attempt mapping (Techno outputs currently identical); Dub evolution admission; qualify audio and obtain three blind ratings | OPEN |
| G3 edit/Undo | [Fresh P0 host cycle audit](g3-host-audit/README.md): DEVELOP/refusal/cancel/Undo; Melody/FORM/device hash cases still open | Three device cases with canonical/Song hashes: Melody edit, FORM undo, refused/cancelled DEVELOP | OPEN |
| G4 sound | [22-WAV integrity/RMS audit](g4-audio-audit/README.md); DSP-A host/sanitizer evidence; no dry Classic Acid level regression in fresh matched-input renders | User confirmed low Cutoff/Res context; no urgent DSP regression reproduced. Speaker/jack and blind A/B, release/switch/underruns remain | OPEN |
| G6 persistence | [Fresh focused host fault audit](g6-coverage-audit/README.md), including final-ack rejection; New/Clear/patch roundtrips; user three cold cycles and accepted A+B8B on earlier New/Clear image | Current b6c3daf9 BIN not flashed; hashes, three cycles, unaccepted edit exclusion, physical negative SD cases remain. Safe backup route needed without card reader | PARTIAL / OPEN |
| G7 independent UX | Local TYPE/preset functional tests; older scoped SDL screenshots | [Three unfamiliar users](g7-operator-packet.md) without developer coaching, timed complete workflow | OPEN |
| Runtime | Static DRAM184252 B, limit191488 unchanged; historical short logs | Paired #495/candidate CPU/heap/largest/stack, representative30-minute workload | OPEN |
| G5 REAPER | Explicitly deferred | No work for this standalone goal | DEFERRED |
| Independent release review | Local evidence/provenance collected | One final SHA/BIN, complete primary evidence, reviewer sign-off | OPEN |

Release decision: STOP. The active goal is not fully accepted: hardware gates and independent review remain OPEN, G2 has a preregistration mismatch and genre refusal, and no final candidate has complete primary evidence. Historical positive checks remain useful regression evidence; do not discard them or label current G6 data loss based on the corrected omitted-ACCEPT report. The previous “2–8 bars lost” report was the operator forgetting ACCEPT; the user later verified Clear/Reset/Reboot/Load and three cold cycles on an earlier image. This is explicitly not current-BIN G6.

Next device actions are combined in [W1 session](w1-operator-session.md). [Fresh TB303 comparison](tb303-level-audit/README.md) narrows the sound investigation; it does not certify hardware listening. [Owner map](owner-map.md) and R1/R7/R9 three-PR audits remain review work.
