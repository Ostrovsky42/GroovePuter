# GroovePuter 0.9.20 Release Readiness Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Establish evidence-backed release readiness for the exact 0.9.20 candidate, close candidate regressions and user-facing gaps, and prepare the release artifacts without publishing or tagging.

**Architecture:** Work from an isolated checkout of `genre/cell-source`; preserve the exact candidate SHA in all evidence. Resolve failures at the behavior owner, separate inherited test debt from candidate regressions, and keep software, product, and physical-device gates distinct. Release automation may build and stage artifacts, but publication and tagging remain out of scope.

**Tech Stack:** C++/Arduino ESP32-S3 firmware, host C++ tests, Python source-regression checks, GitHub Actions, Cardputer ADV hardware.

**Spec:** `docs/releases/0_9_20_RELEASE.md`, `docs/user/QUICKSTART.md`, `MANUAL.md`, `.github/workflows/release.yml`.

## Global Constraints

- Candidate baseline: `fec3237c95c0e19f61e3958cee7d4cd23cecb483`.
- Do not modify the user's existing dirty checkout.
- Do not claim PASS without direct evidence; missing physical-device evidence stays UNVERIFIED.
- Do not publish a GitHub Release or create a version tag without explicit user authorization.
- Every release artifact must identify its exact source SHA, build profile, and checksums.

## Review Focus

- Partner silence and NoteOff lifecycle while USB/DIN output is blocked or disconnected.
- USB Host role behavior with real OTG keyboard input and reboot-persisted role selection.
- Pattern copy, note range, and step settings at slot, selection, and boundary limits.
- Save, reboot, reload, and continued editing of the same project/material.
- Firmware profile consistency: same FQBN and dynamic-FS1B release build; “MIDI-only” means USB interface mode.

---

### Task 1: Freeze R0 evidence and classify candidate regressions

**Files:**
- Create: `docs/releases/0_9_20_R0_AUDIT.md`
- Read: release notes, README, Quickstart, manual, key map, workflow, relevant tests.

- [x] Record exact local and remote candidate SHA, isolated worktree state, published tag/release state, and available CI runs.
- [x] Record focused suite results and each listed source-regression result on candidate `fec3237c` and its actual parent `fa322ec4`.
- [x] Classify all eight release-draft source-check failures; they were inherited stale source expectations, refreshed narrowly, and now pass on candidate.
- [x] Refreshed inherited stale checks without production changes; source checks alone do not close runtime/hardware gates.

### Task 2: Repair verified candidate regressions using TDD

**Files:**
- Modify only the production owners and targeted tests identified in Task 1.

- [x] R0 found no candidate-only behavioral regression: all eight source-check failures also occur on parent `fa322ec4` and were traced to stale source expectations. No new behavior fix or red/green test was justified by those failures.
- [x] The inherited source checks were refreshed to current ownership and labels without removing the asserted contracts. Their results remain source-level evidence only; SDL and physical gates stay separately classified.
- [x] All eight refreshed checks passed; the fresh `bash scripts/ci/run_core.sh` passed with exit 0; all 46 groups closed and final S1 page-restore passed (log: `.superpowers/sdd/2026-11-04-0-9-20-release-readiness/task-2-tests.log`).
- [x] No source assertion was changed to conceal a candidate-only failure; the export, persistence, MIDI-role and recovery user paths remain open in later gates.

### Task 3: Align the release profile and reproducible build

**Files:**
- Modify: `.github/workflows/release.yml` or `docs/releases/0_9_20_RELEASE.md` only after profile intent is established from existing project contracts.

- [x] Confirmed profile consistency: same FQBN and FS1B build; MIDI-only labels USB interface configuration.
- [ ] Rebuild the final committed candidate and generate checksums/provenance. A same-path clean rebuild comparison is wired into release CI. Cross-path tests found Arduino CLI links cached library objects differently from direct objects; cross-path byte reproducibility remains unverified and must not be claimed.
- [ ] Run the applicable core, MIDI, fixed-DRAM, firmware, and memory gates on that same SHA/profile.
- [ ] Produce the firmware image, `SHA256SUMS.txt`, provenance, and build log; ensure provenance states profile and FS configuration.

### Task 4: Close the first-user path and release documentation

**Files:**
- Modify: `README.md`, `docs/user/QUICKSTART.md`, `MANUAL.md`, `src/ui/docs/keys.md`, and release notes as needed.

- [ ] Walk the documented path from discovery through install, first sound, editing, save, reboot/reload, and export using only user-facing documentation.
- [ ] Add 0.9.20 Partner, note-range, step-settings, USB-role, and known-limits guidance where a new owner needs it.
- [ ] Make compatibility, disabled capabilities, installation method, recovery, support path, and exact artifact links unambiguous.
- [ ] Keep unverified features and hardware outcomes visibly marked UNVERIFIED.

### Task 5: Complete software and physical acceptance; prepare decision packet

**Files:**
- Update: `docs/releases/0_9_20_RELEASE.md`, `docs/releases/0_9_20_R0_AUDIT.md`, evidence templates/artifacts.

- [ ] Run and record all release workflow gates on the final SHA.
- [ ] Execute the exact Cardputer ADV checklist for Partner ANSWER/LOOP, silence, step settings, copy, C#5/E5, USB role, persistence, and recovery; attach device/build identity and logs.
- [ ] Record unresolved risks with severity and exact user impact; exception requires explicit owner decision.
- [ ] Prepare release notes, binaries, checksums, provenance, and a draft publication description. Stop before tag or publication.
