# G6 Save As rollback after secondary I/O failure

Reproduced on integration HEAD a0ad73df plus the new Save test: when Scene writing fails and reverting the selected project also fails to persist, Save As returns false but retains the target namespace. Test injection follows setCurrentSceneName's failure contract: retain the pre-call namespace on failed pointer persistence. RED: exit134 at `storage.getCurrentSceneName()=="source"`.

Fix: in both Save As failure exits, try the existing durable rollback, then use existing restoreSceneSelection on failure. This restores selected project and Pattern Paging namespace in RAM without requiring a working filesystem. No new owner/queue/allocation or serialization format. Main Save still returns false. A durable boot pointer cannot be guaranteed when storage refuses writes; reboot recovery under that failure remains OPEN. Partial target files are not asserted absent.

GREEN: full `tests/run_g6_failed_load_tests.sh` exit0 with the extended test, including fault removal and successful reload. Experimental DUAL OSC SDL build exit0. Cardputer ADV build exit0; data41996 + bss142256 =184252 B, unchanged limit191488. New BIN and SHA-256 are in save-rollback-build.json; not flashed.

First ADV invocation incorrectly overrode compiler.cpp.extra_flags, replacing the build script's required include path and loop-stack flags; it failed to find song_page_r4_owner.inc. Correct invocation uses the existing GROOVEPUTER_BUILD_EXTRA_CPP_FLAGS environment input. This was an invocation error, with no product build-script change.

Installed device firmware remains BIN349b8ea4… from the previous source; this fix is not flashed and has no physical PASS.

Committed locally as `b6c3daf9fa7a59abadbe3f0b259bea350f809675`. Existing real MiniAcidDisplay/ProjectPage regression for DEVELOP→Save As→Ctrl+Z also passed after the fix: retained Undo receipt and restored Song length, log retained. No remote push/merge.
