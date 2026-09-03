# GF2 Target Validation Contract

GF2-E0 defines one reproducible validation contract for implementation branches after the C2-V0R measurement-foundation checkpoint.

## Exact lineage base

```text
GF2-C2-V0R
commit d719a0b7c0c23a6d91e058a823dd78f8feea9870
tree   ff1d229f538e782c938ce4a2f6c8309f1895036b
```

GF2-E0 changes validation/test/docs infrastructure only. It must not change production musical semantics or start GF2-M0, GF2-I1, or GF2-C2 Gate B.

## Before / gap evidence

Before E0, the repository already had proven target commands, primarily in `.github/workflows/core-regressions.yml` and specialized 0.9.9 workflows:

- host tests existed;
- SDL compilation existed;
- Cardputer ADV compilation existed;
- fixed DRAM validation existed;
- SEQTRAK MIDI-only compilation existed;
- GF2-C2-V0R had its own deterministic host observation workflow.

The missing capability was orchestration, not a new build stack:

1. there was no GF2-specific entry point that produced all mandatory gate results together;
2. target evidence was distributed across independent workflows;
3. a missing target/toolchain result was not represented by a deterministic GF2 PASS / FAIL / UNAVAILABLE matrix;
4. C2-V0R host validation did not itself express SDL/Cardputer/DRAM/SEQTRAK evidence;
5. the 0.9.9-C/I0R regression commands lived directly inside a workflow rather than behind a reusable command.

## Entry point

Local or CI entry point:

```bash
bash scripts/validate_gf2_targets.sh --all
```

Individual gates are also available:

```bash
bash scripts/validate_gf2_targets.sh host
bash scripts/validate_gf2_targets.sh sdl
bash scripts/validate_gf2_targets.sh cardputer
bash scripts/validate_gf2_targets.sh dram
bash scripts/validate_gf2_targets.sh seqtrak
```

The validator does not install packages or download toolchains. Missing local prerequisites are `UNAVAILABLE`, not `PASS`.

Remote entry point:

```text
.github/workflows/gf2-e0-target-validation.yml
```

For pull requests, the workflow checks out and verifies the exact PR head SHA, not the synthetic merge commit. `workflow_dispatch` accepts an explicit `expected_sha`; the workflow checks out that SHA and verifies `git rev-parse HEAD` before validation.

The remote workflow prepares the same already-proven environments used by repository CI:

- Ubuntu build tools + SDL2 + SDL2_gfx;
- `arduino/setup-arduino-cli@v2`;
- `bash scripts/install_arduino_deps.sh` with pinned M5Stack versions.

## Required matrix

### HOST

Authoritative commands:

```bash
bash tests/run_host_tests.sh
bash tests/run_generation_0_9_9_c_tests.sh
bash tests/run_gf2_c2_v0r_tests.sh
```

`tests/run_generation_0_9_9_c_tests.sh` is also used by `.github/workflows/generation-0-9-9-c-activation.yml`, so the workflow and GF2 validator share one regression owner rather than copied compiler commands.

Proves:

- core host regression execution;
- 0.9.9-C bounded activation / GF2-I0R regression remains executable and green;
- GF2-C2-V0R structured observation Cases A/B/C/D and deterministic replay remain executable and green.

Does not prove:

- SDL compilation;
- embedded target compilation;
- physical-device behavior.

PASS: every authoritative host command exits 0.

FAIL: prerequisites exist and an executed host regression command exits non-zero.

UNAVAILABLE: required host executable/tool such as `g++` or `python3` is missing.

### SDL

Authoritative command:

```bash
make -C platform_sdl clean all CXX=g++
```

Proves:

- the production desktop/SDL target compiles with the repository Makefile and expected SDL libraries.

Does not prove:

- interactive UI correctness;
- audio correctness under real user interaction;
- embedded target correctness.

PASS: SDL prerequisites are available and the Make target exits 0.

FAIL: prerequisites are available and compilation exits non-zero.

UNAVAILABLE: SDL toolchain/dependency setup is unavailable or required local SDL metadata/tools are absent.

### CARDPUTER ADV

Authoritative command:

```bash
bash scripts/build.sh --warnings all
```

Proves:

- Cardputer ADV firmware compiles against the pinned/expected embedded toolchain and DRAM-only board configuration.

Does not prove:

- physical boot;
- audio correctness;
- runtime heap behavior;
- timing correctness on device;
- display, keyboard, USB, SD, MIDI, or other peripheral correctness.

PASS: pinned Arduino prerequisites are available and firmware compilation exits 0.

FAIL: prerequisites are available and compilation exits non-zero.

UNAVAILABLE: Arduino CLI, pinned M5Stack dependencies, `rsync`, or remote setup are unavailable.

### FIXED DRAM

Authoritative command:

```bash
bash scripts/check_cardputer_dram_budget.sh build/cardputer-adv-current/GroovePuter.ino.elf
```

Proves:

- the current firmware ELF satisfies the repository's existing fixed static DRAM gate.

Does not prove:

- transient memory safety;
- the unresolved 2292 B vs ~11.4 KB transient-memory invariant;
- physical runtime stability.

PASS: Cardputer ELF exists and the existing DRAM gate exits 0.

FAIL: the ELF exists, required analysis tool is available, and the existing gate rejects it.

UNAVAILABLE: Cardputer build did not PASS in the same full validation run or no usable ELF exists.

GF2-E0 does not change the DRAM threshold or memory policy. Transient-memory policy remains GF2-M0 work.

### SEQTRAK MIDI-ONLY

Authoritative command:

```bash
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

This script already owns the SEQTRAK-specific FQBN and invokes the existing DRAM gate for that build.

Proves:

- the Cardputer ADV SEQTRAK MIDI-only target configuration compiles against the expected embedded toolchain;
- its existing static DRAM gate passes.

Does not prove:

- successful enumeration by a physical SEQTRAK;
- USB host compatibility on actual hardware;
- MIDI behavior or timing on device.

PASS: pinned Arduino prerequisites are available and the authoritative script exits 0.

FAIL: prerequisites are available and the executed script exits non-zero.

UNAVAILABLE: Arduino CLI/pinned dependencies/remote setup are unavailable.

## Status semantics

```text
PASS
    the authoritative command executed and accepted the exact checkout

FAIL
    required prerequisites were available, the authoritative command executed,
    and it returned non-zero

UNAVAILABLE
    the required validation environment or prerequisite was not available,
    or a dependent artifact could not legitimately be produced

NOT RUN
    gate was not requested in this invocation
```

For `--all`, every five mandatory gates must be `PASS` for GF2 target status to be GREEN.

Exit codes:

```text
0  all requested mandatory gates PASS
1  at least one requested gate FAIL
2  no FAIL, but at least one requested gate UNAVAILABLE
```

Outputs:

```text
build/gf2-validation/matrix.txt
build/gf2-validation/matrix.json
```

The remote workflow uploads this directory even when validation is not green, then has a final enforcement step that fails the workflow when the matrix did not pass.

## Local vs remote

Local sandboxes are not required to contain Arduino CLI, pinned M5Stack dependencies, or SDL development packages.

```text
LOCAL
    run the same validator
    missing prerequisites => explicit UNAVAILABLE
    do not install random network dependencies to manufacture a local PASS

REMOTE
    GitHub Actions prepares the proven toolchain/dependency path
    exact checkout SHA is verified before validation
    required target matrix must become all PASS before GF2 implementation is GREEN
```

## Proof boundary

These statements are deliberately different:

```text
TARGET BUILD VALIDATED
    host/SDL/Cardputer/fixed-DRAM/SEQTRAK required matrix is GREEN

HARDWARE VALIDATED
    code was executed and accepted on physical Cardputer ADV hardware
```

GF2-E0 performs build/test validation only.

```text
TARGET BUILD VALIDATED != HARDWARE VALIDATED
```

A successful Cardputer firmware build must never be reported as physical hardware validation.
