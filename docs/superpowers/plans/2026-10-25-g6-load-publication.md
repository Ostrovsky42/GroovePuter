# G6 failed Load publication repair

Authorized scope: reproduced persistence defect under the ratified Critical Review Closure Plan; no new product feature. Base #495 / 64ae4b8b, isolated fix branch.

Root cause: MiniAcid reads/publishes JSON before the resident-page read; Cardputer also acknowledges its read filter after SceneManager publication. Failures can therefore replace canonical data despite a false return.

Use existing SceneManager staging and PatternPaging scratch. Validate/overlay the resident page in staging and run final reader acknowledgement before SceneManager publication. Suppress page-index publication during staging. Preserve existing default low-level parse behavior for non-storage users. Move generated-origin invalidation after successful load. Remove redundant post-publication page reads from successful storage paths. No second Scene, runtime owner, queue or audio allocation.

Checks: negative corrupt primary+backup, final acknowledgement refusal, successful page/backup load and legacy no-page JSON; current canonical/project and indices preserved on refusal. Then existing pruned host/persistence/Undo tests, SDL, ADV and memory comparison. G6 remains OPEN for Melody-file failure, late runtime realization, hardware cold cycles and negative physical SD cases until separately verified.

Software checkpoint: RED→GREEN for corrupt header/CRC and final acknowledgement refusal; current project, canonical JSON, Undo receipt/revision, origin, page indices and output state retained. Existing host suite, filter compatibility, Save-keeps-Undo UI, SDL and ADV PASS. Independent review has no introduced Critical/Important findings. Static DRAM unchanged. Remaining G6 boundaries and physical checks remain OPEN.
