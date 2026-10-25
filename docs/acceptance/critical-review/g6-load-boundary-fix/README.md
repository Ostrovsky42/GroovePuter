# G6 narrow repair — software verified, hardware OPEN

Base: #495 64ae4b8bcf13fc165a04b2b79a80b7e8b3a1f71f. Dedicated local branch fix/20261025-g6-failed-load-preservation. No push/merge or hardware flash.

Two RED cases: corrupt resident page plus backup replaces canonical/project despite refusal; final reader acknowledgement refusal follows Scene publication. GREEN: existing Scene staging now includes validated resident page before publication, followed by final reader acknowledgement. No second Scene or new runtime owner. MiniAcid no longer rereads the page after successful storage reads or invalidates generated provenance before a failed replacement.

Negative tests cover truncated header and full-sized CRC corruption; assert full serialized canonical equality, project name, page indices, valid retained Pattern Undo receipt, Scene revision, generated Phrase recipe and output mode/MIDI-disable epoch. Positive commit and legacy-no-page parsing are exercised. Run bash tests/run_g6_failed_load_tests.sh. Existing six-test pruned host suite, sampler/filter ownership, Save-keeps-Undo UI, SDL and ADV builds PASS.

| Memory | #495 | Fix | Delta |
|---|---:|---:|---:|
| .data | 41996 | 41996 | 0 |
| .bss | 142256 | 142256 | 0 |
| Fixed DRAM | 184252 | 184252 | 0 |
| Flash | 1591658 | 1592182 | +524 |

Existing DRAM limit 191488 unchanged. Runtime heap, stack and timing NOT TESTED on fix. Staging reuses the existing scratch; page I/O now occurs while the parser observer is live, so hardware stack/heap measurement is still required. Synthetic final-ack refusal does not certify the physical Cardputer reader. G6 stays OPEN for missing/corrupt accepted Melody files, late runtime realization, physical SD failures and cold restart cycles. Power-cut atomicity UNVERIFIED. #495 baseline remains installed on hardware. No full transaction/persistence acceptance claim.
