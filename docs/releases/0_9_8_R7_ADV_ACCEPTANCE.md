# GroovePuter 0.9.8 R7 — ADV acceptance and release freeze

R7 is the acceptance/freeze stage for the 0.9.8 Undo Safe Editing line. It does not add another editing feature or another Undo owner.

## Immutable R7 baseline

R7 starts from the R6 squash merge on `dev_0.9.8`:

```text
5d3b1d2956952a1faccf9cbf96c1fce19650c642
```

The final R7 candidate SHA is recorded in the pull-request evidence after all tracked R7 files are complete. Do not write the candidate SHA into this file: changing this file would create a different SHA and invalidate the statement.

## Scope boundary

Accepted 0.9.8 Undo remains one-level and bounded by the canonical fixed-size owner. Pattern, Song and Phrase restore ownership stays where R2-R6 established it. R7 does not add redo, multi-level history, Scene snapshots, heap-backed history, scheduler ownership, queued activation, or another receipt decoder.

**Generation/activation Undo remains a 0.9.9 boundary.** R7 must not claim generation Undo support merely because generation paths compile or their unrelated workflows are green.

## Software acceptance

The exact R7 candidate must pass all of the following without moving the candidate SHA:

- cumulative `tests/run_undo_0_9_8_r7_tests.sh`;
- repository Core host regressions;
- SDL desktop build;
- Cardputer ADV normal firmware compile;
- Cardputer ADV fixed-DRAM policy check;
- Cardputer ADV SEQTRAK MIDI-only compile;
- all pre-existing R2-R6 Undo contracts that are triggered on that SHA.

A green compile is evidence that the firmware builds for the selected target. It is not evidence that a physical Cardputer booted, that a keyboard chord was electrically observed, or that runtime heap fragmentation stayed stable during a long session.

## DRAM evidence status

The repository currently enforces `scripts/check_cardputer_dram_budget.sh` in the ADV Core job. Its default static ceiling is `191488` bytes. The script itself explicitly labels that threshold a **provisional exception** and states that threshold-rule items 5-7 — hardware minima, declared reserves and the deriving calculation — remain pending.

R7 therefore preserves two separate claims:

1. **CI DRAM policy:** PASS only when the exact candidate is below the repository's currently enforced ceiling.
2. **Hardware memory acceptance:** PENDING until runtime measurements on the physical ADV establish the required minima/reserves and show no progressive loss during soak.

Do not collapse claim 1 into claim 2.

## Physical Cardputer ADV checklist

Hardware state: PENDING DEVICE

Run this checklist on the exact final candidate firmware. Record observations against that exact SHA before release/tag acceptance.

1. Cold boot the Cardputer ADV and verify normal UI/audio startup.
2. Pattern: make an ordinary edit, press **Ctrl+U**, verify the prior state is restored and the UI reports `UNDO: PATTERN`.
3. Pattern destructive path: clear or paste, then **Ctrl+U**; verify exactly one prior state is restored.
4. Song: exercise Cut/Paste/Delete or equivalent accepted arrangement mutations, then **Ctrl+U**; verify `UNDO: SONG` and correct prior cells.
5. Phrase: exercise accepted INSERT and REPLACE/write paths, then **Ctrl+U**; verify `UNDO: PHRASE` or `UNDO: SONG` according to the established owner.
6. One-level replacement: perform mutation A, then mutation B, then Undo; verify B is restored and A is not exposed as a second history level.
7. Repeated Undo: after consuming the retained receipt, press **Ctrl+U** again and verify empty/no-op semantics rather than a stale restore.
8. Cross-page retention: create a valid receipt, move away from its owning page, press **Ctrl+U** and verify `UNDO: RETURN PAGE`; return to the owner and verify the receipt is still usable.
9. Shortcut collision: on Synth Sound, verify **Ctrl+Z** still resets the selected parameter and does **not** invoke global Undo. Global Undo is **Ctrl+U**.
10. Dirty/revision semantics: after a successful Undo, verify the project/scene revision behavior matches the accepted R2-R6 contract and persistence detects the restored mutation correctly.
11. Playback safety: repeat representative Undo operations while stopped and during safe playback conditions; verify no stuck notes, transport corruption or unexpected output-profile changes.
12. SEQTRAK profile: boot/use the MIDI-only profile sufficiently to confirm the R7 UI change did not alter the established profile/output contract.
13. Runtime memory: record free internal heap and largest free internal block after boot, after representative UI/edit/playback activity, and during the soak.
14. **30-minute soak**: exercise navigation, Pattern/Song/Phrase edits, Undo, playback and the expected MIDI profile. Verify no progressive heap loss, fragmentation trend, lock-up, reboot or audio/MIDI degradation.

## Release rule

**DO NOT TAG / DO NOT CLAIM RELEASE ACCEPTED** while `Hardware state: PENDING DEVICE` remains true or while the provisional DRAM threshold lacks the physical evidence required by the release policy.

The R7 PR may be merged when its software-only changes and exact-SHA CI are green because the PR itself only installs the acceptance contract/checklist. That merge does **not** convert the physical checklist to PASS and does not authorize a 0.9.8 release/tag.

Once device evidence exists, record the tested release-candidate SHA, firmware/profile, heap/largest-block observations, soak outcome and checklist result in a follow-up release-evidence change. A release claim must refer to the SHA that was actually exercised.
