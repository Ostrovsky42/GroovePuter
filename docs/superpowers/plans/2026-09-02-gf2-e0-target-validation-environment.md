# GF2-E0 Target Validation Environment Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Provide one reproducible GF2 validation entry point that classifies HOST, SDL, Cardputer ADV, fixed DRAM, and SEQTRAK MIDI-only as PASS, FAIL, or UNAVAILABLE for an exact commit without changing production musical semantics.

**Architecture:** Reuse the existing repository build/test owners rather than introducing a parallel build system. Centralize the existing 0.9.9-C host regression commands into a reusable test script, then add a thin GF2 orchestration script that runs the authoritative commands and emits deterministic text/JSON evidence. A dedicated GitHub Actions workflow checks out an explicitly requested SHA, prepares the already-proven SDL and Arduino toolchains, executes the same orchestration script, uploads the matrix, and fails when any mandatory gate is FAIL or UNAVAILABLE.

**Tech Stack:** Bash, GitHub Actions, existing g++/Make/SDL2 toolchain, existing Arduino CLI/M5Stack scripts.

**Spec:** GF2-E0 task supplied in conversation on 2026-09-02.

## Global Constraints

- Exact base: `d719a0b7c0c23a6d91e058a823dd78f8feea9870`.
- Exact base tree: `ff1d229f538e782c938ce4a2f6c8309f1895036b`.
- Production musical semantic delta: NONE.
- No `src/` changes.
- Do not start GF2-M0, GF2-I1, or GF2-C2 Gate B.
- Cardputer/SEQTRAK compilation is target-build validation, not physical hardware validation.
- UNAVAILABLE / NOT RUN must never be converted to PASS.
- Existing authoritative build commands remain owners.

---

### Task 1: Centralize the existing 0.9.9-C / GF2-I0R host regression owner

**Files:**
- Create: `tests/run_generation_0_9_9_c_tests.sh`
- Modify: `.github/workflows/generation-0-9-9-c-activation.yml`

**Interfaces:**
- Consumes: existing tests `test_generation_activation_0_9_9.cpp`, `test_generation_activation_0_9_9_source.py`, `test_generation_undo_owner_0_9_9.cpp`, `test_generation_undo_owner_0_9_9_source.py`, `test_pattern_generation_owner_0_9_9.cpp`, `test_pattern_generation_owner_0_9_9_source.py`, `run_undo_0_9_8_r7_tests.sh`, `test_generation_0_9_9_compatibility.cpp`, and `test_generation_0_9_9_source_regressions.py`.
- Produces: `bash tests/run_generation_0_9_9_c_tests.sh` as the reusable authoritative command for the 0.9.9-C activation/I0R regression chain.

- [ ] **Step 1: Preserve current behavior as the baseline contract**

Current workflow commands are the baseline and must be moved without semantic changes. The new script must compile/run the activation owner test, source contract, B1/B2 owner tests, cumulative Undo tests, compatibility test, and source regressions with the same compiler flags.

- [ ] **Step 2: Create the reusable script**

Create `tests/run_generation_0_9_9_c_tests.sh` with `set -euo pipefail`, repository-root discovery, `build/host-tests` creation, and the exact existing commands in their current order.

- [ ] **Step 3: Replace workflow duplication with the script call**

Change `.github/workflows/generation-0-9-9-c-activation.yml` so its single validation step runs:

```bash
bash tests/run_generation_0_9_9_c_tests.sh
```

Do not change trigger semantics or production code.

- [ ] **Step 4: Verify the reusable regression owner**

Run:

```bash
bash tests/run_generation_0_9_9_c_tests.sh
```

Expected: exit 0.

- [ ] **Step 5: Commit**

```bash
git add tests/run_generation_0_9_9_c_tests.sh .github/workflows/generation-0-9-9-c-activation.yml
git commit -m "test(gf2): centralize activation regression owner"
```

---

### Task 2: Add the GF2 target validation entry point and exact-SHA remote workflow

**Files:**
- Create: `scripts/validate_gf2_targets.sh`
- Create: `.github/workflows/gf2-e0-target-validation.yml`
- Create: `docs/gf2/GF2_TARGET_VALIDATION.md`

**Interfaces:**
- Consumes:
  - HOST: `bash tests/run_host_tests.sh`, `bash tests/run_generation_0_9_9_c_tests.sh`, `bash tests/run_gf2_c2_v0r_tests.sh`.
  - SDL: `make -C platform_sdl clean all CXX=g++`.
  - CARDPUTER ADV: `bash scripts/build.sh --warnings all`.
  - FIXED DRAM: `bash scripts/check_cardputer_dram_budget.sh build/cardputer-adv-current/GroovePuter.ino.elf`.
  - SEQTRAK MIDI-only: `bash scripts/build_seqtrak_midi_only.sh --warnings all`.
- Produces:
  - `bash scripts/validate_gf2_targets.sh` as the single local/CI GF2 validation entry point.
  - `build/gf2-validation/matrix.txt` and `build/gf2-validation/matrix.json`.
  - `.github/workflows/gf2-e0-target-validation.yml` as the remote exact-SHA validator.

- [ ] **Step 1: Record BEFORE/gap evidence**

Document that existing target commands are proven but scattered across `core-regressions.yml` and specialized workflows; C2-V0R itself is host-only; there is no exact-SHA GF2 orchestration contract and no deterministic five-gate PASS/FAIL/UNAVAILABLE matrix.

- [ ] **Step 2: Implement deterministic gate classification**

`scripts/validate_gf2_targets.sh` must:

1. accept `--all` (default) and individual gate names `host`, `sdl`, `cardputer`, `dram`, `seqtrak`;
2. run all requested gates even after one fails;
3. classify missing local prerequisites as `UNAVAILABLE`;
4. classify an executed authoritative command returning non-zero as `FAIL`;
5. classify success as `PASS`;
6. make DRAM `UNAVAILABLE` when Cardputer firmware was not successfully built in the same run and no expected ELF exists;
7. emit deterministic matrix text and JSON;
8. exit 0 only when all requested mandatory gates are PASS, 1 when any gate FAILs, and 2 when no gate FAILs but at least one gate is UNAVAILABLE.

The script must not install dependencies or download toolchains.

- [ ] **Step 3: Add exact-SHA remote validation workflow**

`.github/workflows/gf2-e0-target-validation.yml` must support `pull_request` and `workflow_dispatch`. For dispatch, require an `expected_sha` input. Checkout the exact PR head SHA or requested SHA rather than the synthetic PR merge commit, verify `git rev-parse HEAD` equals the expected SHA, then prepare the existing SDL and Arduino environments using:

```text
sudo apt-get install build-essential libsdl2-dev libsdl2-gfx-dev
arduino/setup-arduino-cli@v2
bash scripts/install_arduino_deps.sh
```

Setup steps must expose failure to the validator as UNAVAILABLE rather than silently passing. The validator step may use `continue-on-error: true` only so the matrix artifact can still be uploaded; a final enforcement step must make the workflow fail whenever the validator was not successful.

- [ ] **Step 4: Document proof boundaries**

`docs/gf2/GF2_TARGET_VALIDATION.md` must list every gate's authoritative command, what it proves, what it does not prove, PASS/FAIL/UNAVAILABLE semantics, local-vs-remote expectations, and explicitly state:

```text
TARGET BUILD VALIDATED != HARDWARE VALIDATED
```

- [ ] **Step 5: Run available local validation**

Run:

```bash
bash scripts/validate_gf2_targets.sh --all
```

Record actual local PASS/FAIL/UNAVAILABLE results without installing missing network dependencies.

- [ ] **Step 6: Push and run remote exact-SHA validation**

Open a Draft stacked PR against `research/20260901-02-0.9.10-gf2-c2-v0r-structured-observation`. Run the dedicated workflow on the final branch head. Record workflow name, run id, job names, result, and exact head SHA.

- [ ] **Step 7: Verify regressions and scope**

Confirm:

```text
GF2-I0R regression       PASS
GF2-C2-V0R regression    PASS
HOST                      PASS
SDL                       PASS
CARDPUTER ADV             PASS
FIXED DRAM                PASS
SEQTRAK MIDI-ONLY         PASS
src/ changes              NONE
GF2-M0                    NOT STARTED
GF2-I1                    NOT STARTED
GF2-C2 Gate B             NOT STARTED
HARDWARE VALIDATED        NO
```

- [ ] **Step 8: Commit final orchestration**

```bash
git add scripts/validate_gf2_targets.sh .github/workflows/gf2-e0-target-validation.yml docs/gf2/GF2_TARGET_VALIDATION.md
git commit -m "ci(gf2): add exact-sha target validation matrix"
```
