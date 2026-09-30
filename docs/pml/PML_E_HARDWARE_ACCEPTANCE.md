# PML-E — hardware acceptance candidate and procedure

Status: **candidate `b4346c8f` prepared; NOT accepted.** No device run has happened for this candidate. (`839f4563` was flashed first and is superseded: an SDL screenshot pass found the ALLOW REPLACEMENT layout clipped by the shell HUD; `b4346c8f` fixes that.) Memory remains a **conditional pass**, not GREEN.
Scope: PML-C (session-only allow-replacement marks) with the ALLOW REPLACEMENT UI. **Not closed by this checkpoint:** repeated GROW, development of a manually edited A,
variants by salt, an exact preview of the selected GROW, musical expressiveness.

## 1. Candidate record

| Item | Value |
|---|---|
| Full SHA | `b4346c8f230d0ef0fad6076c2b9880674a8765bc` (dirty tracked files: 0) |
| Branch | `feature/20260929-m0-musical-play-baseline` (not merged to `dev`) |
| Commits in this candidate line | F1 `679a28fa`, UI `fa3947b5`, label `8f518689`, probe `839f4563`, docs `a96e4408`/`a3fe5040`, layout fix `b4346c8f` |
| Last **full** serial run | 18/18 green at `fa3947b5` (D1-A/B/B1/C/C1, cycle, mix, PML-C, PML-E, P0-B, M0-A, PMB-P1, host, unified slots, SDL, ADV, SEQTRAK, FS1B) |
| Gates run **at this SHA** | PML-E UI, cycle, host, unified slots, SDL, ADV, SEQTRAK, FS1B, probe build: all exit 0 (`logs/candidate_b4346c8f_gates.txt`). The other ten gates were **not** re-run at this SHA: every commit after `fa3947b5` changes `phrase_page.cpp` (label, flag-only probe lines, layout) and its test, or documents. |
| FS1B | `=== FS1B dynamic-FatFs Cardputer build PASS ===` for both images below |
| Static DRAM | **189984 B** of a 191488 B budget, headroom **1504 B**; `.dram0.data` 37080 B, `.dram0.bss` 152904 B; identical for both images (the probe adds no static RAM) |
| Memory status | **Conditional pass.** The check passes under a *provisional exception*: its ceiling is a rollback to an older repository ceiling and "not a universal hardware safety boundary"; threshold-rule items 1-4 are met and **items 5-7 (hardware minima, declared reserves, the deriving calculation) are pending**. |

### Images (same SHA, separate builds)

| Image | Purpose | ELF sha256 | BIN sha256 | Files |
|---|---|---|---|---|
| **Default** | the product image; `[PML-PROBE]` strings absent (verified: 0 in the ELF) | `e66db3da2b20d9e6cd2f93d41cb8df70a05dea15fb5140da97bddbb3d3869e41` | `961aed219fe5612662a680c6fbfd922b9eefd06c897b4925eb1ace04f43f78de` | `build/candidates/default-b4346c8f/` |
| **Probe** | the device-acceptance image: same code plus `-DGROOVEPUTER_P0_CYCLE_PROBE` (1 `[PML-PROBE]` string in the ELF) | `60cee26f78828ede18433a19ea3141d159e16b2a95278f6c934b183603be1e7f` | `25978aa27a385fd62d1ba45d375b5bb01ab20dc7cf03e702d022612fc88f8dc9` | `build/candidates/probe-b4346c8f/` |

Logs: `docs/pml/logs/candidate_b4346c8f_gates.txt`, `..._default_image.txt`, `..._probe_image.txt` (DRAM check verbatim); the superseded `839f4563` logs stay beside them. Older evidence: `fs1b_pmlc_dram_check.txt` (PML-C build).
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
* **Layout was verified in the SDL emulator** (window 480x270 = 2x of 240x135): the shell paints its HUD strip over content line 7, so the ALLOW REPLACEMENT views use lines 0-6 only. Host tests cannot see this (text width is approximated); the device screen is still the final judge.
* An exact preview of the selected GROW (will it run, and with 4 or 8 bars) is still an open requirement; the two `SLOT SPACE` lengths are a capacity preview only.
