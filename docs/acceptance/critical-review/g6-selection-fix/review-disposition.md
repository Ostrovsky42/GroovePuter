# Independent review disposition — G6 selection repair

## Prior review comment

The independent reviewer flagged that `beginSceneLoad()` temporarily selects the target project name and `PatternPagingService` namespace before the target has completed validation, classifying this as P1. This is a real visibility/staging concern, but the finding applies a stronger invariant than the ratified acceptance plan, which requires that a failed Load leave the existing canonical project undamaged.

The production Load action is executed inside the Cardputer `AudioGuard`, which pauses the renderer for the synchronous operation. The target namespace is required to read and validate its page and Melody files. On failure, `loadSceneByName()` restores the prior in-memory scene and paging names without persisting the target boot pointer or writing files. Existing host fault-injection tests assert canonical Scene, output mode, storage name, paging namespace, and boot pointer preservation. The finding is therefore recorded as a design boundary, not silently dismissed; asynchronous observers or future load refactors must preserve the audio-guard isolation or stage all names separately.

## Additional UI defect discovered during disposition

The actual ProjectPage Load path wrapped the operation in `withAudioGuard()`, whose generic completion logic marks the scene mutated even when Load returns false. A user-visible refused Load therefore advanced the dirty revision despite leaving canonical data unchanged. This was not covered by the earlier engine-level failure test.

- Regression: `tests/test_project_usb_role_ui.cpp`, using `FailTargetLoadStorage` and the actual ProjectPage Load interaction.
- RED: assertion that `currentRevision` remains 21 failed before the fix; see `project-ui-failed-load-RED.log`.
- GREEN: `tests/run_project_usb_role_ui_tests.sh` PASS; failed Load preserves canonical JSON, project name, current revision and persisted revision.
- Fix: run the Load callback through the audio pause guard directly; only the explicit successful Load/recovery paths publish revision state.

## Remaining review boundary

This disposition is not the independent final review for the release candidate. Device Save→Reset→Load and SD-failure cases remain OPEN, as do runtime and release gates. No push or merge was performed.
