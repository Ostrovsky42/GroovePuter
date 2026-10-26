# G6 coverage audit — 2026-10-26

Scope: existing integration production source 9e6c3ddf, repository HEAD a0ad73df, default six-engine pruned host archive. Five focused executions compiled and run freshly; all exit 0. No production changes. These results do not transfer to a physical G6 PASS.

| Requirement / failure | Actual coverage | Remaining boundary |
| --- | --- | --- |
| Final streaming-filter acknowledgement fails | `test_g6_failed_load_preserves_scene`: late failed acknowledgement returns false; canonical serialized Scene equals before; positive control commits target | Real Cardputer filtered storage/SD fault still OPEN |
| Resident page malformed header / CRC | Same test, default and crc invocations: failed Load preserves canonical Scene, active page, generated recipe, revision/persisted revision, Undo receipt, output mode and MIDI-disable epoch | Hardware negative storage cases OPEN |
| Target read fails / boot-pointer commit fails | `test_g6_selection_rollback`: source Scene, selected namespace, paging namespace and output retained; backup pointer recovery and positive target Load asserted | Real card removal/write errors and reboot recovery OPEN |
| Accepted A+B 8B files damaged after successful preparation | `test_g6_late_hydration`: complete prepared target retained without a second hydration read | Physical 8B/Song/HGS/patch hash cycles OPEN |
| Same files damaged before preparation | prepare-fail invocation: Load false, old Scene and unaccepted working Melody versions retained | Physical negative test OPEN |
| Save no false success | Save returns false when page save, Scene write, or recovery cleanup fails by current source inspection | New `test_g6_failed_save` injects Scene-write and recovery-cleanup failures through the public Save As API, asserts false, failed-target pointer/namespace rollback, main-file preservation on write refusal, and successful recovery/reload. Page-save failure and physical Cardputer paths remain OPEN |
| Save As/New/Clear | Existing earlier host/device evidence retained in integration dossier | Exact current BIN physical checks OPEN |
| Power-cut atomicity | Not exercised | UNVERIFIED |

The final-acknowledgement boundary in the closure plan has a direct negative host test, not only a source-order assertion. The five fresh logs and compile commands in results.json are retained. This audit is narrow and does not requalify other G6 cases from the full runner. Existing host archive was reused because production source has not changed; no new default-vs-DUAL equivalence is claimed.

Additional focused Save fault test: PASS. Added to the existing G6 runner. UI toast success/failure branching was inspected in ProjectPage::confirmSave (success and persisted revision only when saved=true), but the new test does not simulate screen input or claim UI/hardware acceptance. Cleanup failure can leave the main project durably saved while reporting failure; the test explicitly permits this conservative outcome. No production source was changed.

Full runner subsequently completed with exit 0 on the working test addition: `bash tests/run_g6_failed_load_tests.sh`. Includes Cardputer prune compatibility, pointer backup, staged Scene/CRC/selection rollback, resident/nonresident Melody and page faults, read-size sanitizer, late preparation, New/Clear and failed Save. Compressed primary log and result manifest retained. Does not imply physical SD, DUAL OSC sanitizer or complete release acceptance.
