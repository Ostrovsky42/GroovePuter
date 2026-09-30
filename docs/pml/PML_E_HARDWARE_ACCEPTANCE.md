# PML-E — hardware acceptance candidate and procedure

Status: **candidate prepared; NOT accepted.** No device run has happened for this candidate. Memory remains a **conditional pass**, not GREEN.
Scope: PML-C (session-only allow-replacement marks) with the ALLOW REPLACEMENT UI. **Not closed by this checkpoint:** repeated GROW, development of a manually edited A,
variants by salt, an exact preview of the selected GROW, musical expressiveness.

## 1. Candidate record

| Item | Value |
|---|---|
| Full SHA | `839f4563158e96369b66dc6d80d4c3d555b91eee` (dirty tracked files: 0) |
| Branch | `feature/20260929-m0-musical-play-baseline` (not merged to `dev`) |
| Commits in this candidate line | F1 `679a28fa`, UI `fa3947b5`, label `8f518689`, probe `839f4563` |
| Last **full** serial run | 18/18 green at `fa3947b5` (D1-A/B/B1/C/C1, cycle, mix, PML-C, PML-E, P0-B, M0-A, PMB-P1, host, unified slots, SDL, ADV, SEQTRAK, FS1B) |
| Gates run **at this SHA** | PML-E UI, cycle, host, unified slots, SDL, ADV, SEQTRAK, FS1B, probe build: all exit 0 (`logs/candidate_839f4563_gates.txt`). The other ten gates were **not** re-run at this SHA: the two commits after `fa3947b5` change `phrase_page.cpp` (a label and flag-only probe lines) and its test only. |
| FS1B | `=== FS1B dynamic-FatFs Cardputer build PASS ===` for both images below |
| Static DRAM | **189984 B** of a 191488 B budget, headroom **1504 B**; `.dram0.data` 37080 B, `.dram0.bss` 152904 B; identical for both images (the probe adds no static RAM) |
| Memory status | **Conditional pass.** The check passes under a *provisional exception*: its ceiling is a rollback to an older repository ceiling and "not a universal hardware safety boundary"; threshold-rule items 1-4 are met and **items 5-7 (hardware minima, declared reserves, the deriving calculation) are pending**. |

### Images (same SHA, separate builds)

| Image | Purpose | ELF sha256 | BIN sha256 | Files |
|---|---|---|---|---|
| **Default** | the product image; `[PML-PROBE]` strings absent (verified: 0 in the ELF) | `2f03d658d2fe6867316f939f6b90b5c5a9226335159011ebe16bc0e49d0af821` | `0bd147f480800ba7c38e7c986b273a0c8c533e38f69774aaf63f0be0132629ca` | `build/candidates/default-839f4563/` |
| **Probe** | the device-acceptance image: same code plus `-DGROOVEPUTER_P0_CYCLE_PROBE` (1 `[PML-PROBE]` string in the ELF) | `53d971cbc63e73d0a2d2dce2ef155ca4537937c6ede633a4698aabc5ea3a0b45` | `c25ba3ceec67a669793aaae453b1cfe96f9f2325b51c7be185f8a3d468b7f84e` | `build/candidates/probe-839f4563/` |

Logs: `docs/pml/logs/candidate_839f4563_gates.txt`, `..._default_image.txt`, `..._probe_image.txt` (DRAM check verbatim). Older evidence: `fs1b_pmlc_dram_check.txt` (PML-C build).
Flash only the image the owner names; the probe image is the one for this acceptance.

## 2. Device procedure (owner)

Serial monitor on the USB port shows `[PML-PROBE] <action> stackMinFreeBytes=… internalFree=… largestBlock=… minEverFree=…` after each action below
(`R-open`, `R-allow`, `R-cancel`, `G`, `UNDO`) and `[P0-CYCLE-PROBE] …` after each D. `stackMinFreeBytes` is the minimum free stack **since the task started**, so a later line
also covers every earlier draw and handler. Copy the lines into the report.

**A. The original path (House).**
1. House / rave as usual; MATERIAL, LENGTH 4B. Press G until a TAKE is refused with `NO ROOM: R=ALLOW REPLACE` (or fill the page another way).
2. Edit one generated pattern by hand; delete the Song rows of the unwanted TAKEs.
3. G again: expect the refusal; product line reads `NO SLOTS: R`.
4. **R**: grid; check the letters (`~` unused, `C` CURRENT, `U` live Undo, `S` song-referenced) and `SLOT SPACE` lines (`TAKE 4B  NO / NO` before any permission).
5. **ENTER** on a `~` slot: the confirmation must say *"AFTER REPLACEMENT UNDO WILL NOT RESTORE THE OLD CONTENT"* and *"NOTHING IS ERASED NOW"*. **ESC** cancels: no `*` appears.
6. ENTER, ENTER: allowed (`*`). Allow four slots. `TAKE 4B  NO / YES`. On a held slot ENTER only names the holder (`HELD BY …`).
7. R back: TO line `REPLACES ALLOWED`. **G**: a new TAKE is committed into the allowed slots and plays with all its notes.
8. **Undo** (Ctrl+Z): `UNDO: MATERIAL`; the replaced slots are empty, the old content does **not** return, the Song rows are back as before the TAKE.
9. Edited slot: allow a slot, edit it by hand in the pattern editor, open R: it reads `EDITED, ALLOW AGAIN?` and G does not replace it.
10. Save the project, power off, cold start, load: the new content is present, the permissions are gone (R shows `~`, not `*`), the old content is absent.

**B. A successful GROW (separate scenario, admitted material).** Techno, STYLE REWORK, G, D until a GROW is published (House cannot GROW: `TRY ANOTHER TAKE: G`); Undo once; D again.

**C. Memory.** Record the `[PML-PROBE]` and `[P0-CYCLE-PROBE]` lines from every step above, and note any audio dropout or unresponsive UI.

## 3. What the acceptance does and does not decide

* Decides: the PML-C/PML-E behaviour on the device, Undo correctness, save/cold-start, and whether the recorded stack/heap minima leave room.
* Does **not** by itself turn memory GREEN: the provisional exception and pending items 5-7 stay until the owner's threshold derivation exists.
* An exact preview of the selected GROW (will it run, and with 4 or 8 bars) is still an open requirement; the two `SLOT SPACE` lengths are a capacity preview only.
