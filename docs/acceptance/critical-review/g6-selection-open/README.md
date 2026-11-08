# G6 failed Load selection rollback — FAIL / not yet repaired

Reproduced after nonresident repair: ReadFailureStorage injects a read failure after target selection; independently block boot-pointer writes (directory at pointer path) or make the SD root unavailable (regular file used as SD root). Load returns false and canonical JSON remains source A, but SceneStorage and PatternPaging names remain target B. Subsequent Save can target the wrong project. This is host fault injection, NOT physical SD evidence.

Root cause: setCurrentSceneName publishes selection/boot pointer before candidate validation; error handling calls the same I/O-dependent setter to restore A. If I/O fails, rollback itself restores B.

Required narrow repair:

- Begin a Load selection without publishing the boot pointer, retaining existing directory/legacy migration admission checks.
- Restore SceneStorage and paging namespace in memory without requiring SD; original name is already retained by MiniAcid, so no new persistent owner/state.
- Publish the boot pointer only after page/material/selected-Melody preparation and read-filter validation, before canonical publication. Keep ordinary SaveAs/New setters separate.
- Cardputer read filter needs a nonpublishing final-validation entry point; output modes/sample refs must not publish if pointer commit fails. Preserve default failed() behavior for existing callers.
- SDL pointer replacement should use verified temporary output + rename, not truncate the old pointer on an unsuccessful write.
- Boot storage reads need not publish a new selection; use an explicit default-false read option for explicit Load only rather than adding sticky transaction state.

Do not merely fix the RAM name while retaining a prematurely published boot pointer. Preserve legacy project migration and JSON-only load compatibility. No new runtime owners/queues, no second Scene. RED cases and exact source attached. No production repair made yet; release remains STOP.
