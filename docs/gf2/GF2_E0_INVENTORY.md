# GF2-E0 Validation Inventory

## Exact base

```text
commit d719a0b7c0c23a6d91e058a823dd78f8feea9870
tree   ff1d229f538e782c938ce4a2f6c8309f1895036b
```

## Current validation topology

### REUSE — authoritative owners

- `.github/workflows/core-regressions.yml`
  - established CI evidence for host regressions, SDL build, Cardputer ADV build, fixed DRAM gate, and SEQTRAK MIDI-only build.
- `tests/run_host_tests.sh`
  - existing core host regression entry point.
- `tests/run_gf2_c2_v0r_tests.sh`
  - GF2-C2-V0R deterministic structured observation owner.
- `.github/workflows/generation-0-9-9-c-activation.yml`
  - 0.9.9-C bounded activation / GF2-I0R regression owner.
- `platform_sdl/Makefile`
  - authoritative SDL compilation owner.
- `scripts/build.sh`
  - authoritative Cardputer ADV firmware build owner.
- `scripts/check_cardputer_dram_budget.sh`
  - authoritative fixed static DRAM gate.
- `scripts/build_seqtrak_midi_only.sh`
  - authoritative SEQTRAK MIDI-only build owner; already calls the fixed DRAM gate for its own ELF.
- `scripts/install_arduino_deps.sh`
  - pinned M5Stack dependency installation owner for CI.

### EXTEND

- 0.9.9-C workflow command body is extracted to `tests/run_generation_0_9_9_c_tests.sh` so both its workflow and GF2-E0 can call one owner.
- New GF2 orchestration calls the authoritative commands and classifies results; it does not reimplement their build logic.
- New GF2 remote workflow prepares the already-proven dependencies and invokes the same orchestration entry point on an exact checkout SHA.

### OBSOLETE

None identified for removal in E0. Existing specialized workflows continue to provide their narrower regression evidence.

### NOT RELEVANT

- physical flashing/monitor workflows: E0 is build validation, not hardware acceptance;
- WASM/bundle targets: not part of the mandatory GF2 matrix;
- GF2-M0 transient-memory policy: intentionally out of scope.

## Current duplication

The same 0.9.9-C activation/I0R compiler/test commands were embedded directly in a workflow and therefore were not reusable by a GF2 aggregate validator. E0 removes that duplication by moving them behind one test script.

Target build commands themselves are not duplicated into new build implementations. E0 only invokes the existing SDL Makefile and existing Cardputer/DRAM/SEQTRAK scripts.

## Current local limitations

A generic sandbox may have host compilers but lack SDL development packages, Arduino CLI, pinned M5Stack core/libraries, or network access. Missing prerequisites are therefore a first-class `UNAVAILABLE` result. The E0 validator never installs dependencies itself.

## Current remote capability

GitHub Actions already demonstrates a stable setup path:

```text
SDL:
  apt install build-essential libsdl2-dev libsdl2-gfx-dev

Cardputer / SEQTRAK:
  arduino/setup-arduino-cli@v2
  bash scripts/install_arduino_deps.sh
```

The E0 workflow reuses this setup and then invokes existing repository build owners.

## Minimal E0 delta

```text
1. reusable 0.9.9-C/I0R test script
2. one GF2 validation orchestration entry point
3. one exact-SHA remote workflow
4. deterministic PASS / FAIL / UNAVAILABLE matrix artifact
5. proof-boundary documentation
```

No production C++ change is required.
