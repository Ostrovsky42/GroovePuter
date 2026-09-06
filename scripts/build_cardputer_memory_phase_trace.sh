#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Per-owner startup memory accounting for the SD runtime residency workstream.
# Adds only the GROOVEPUTER_MEMORY_PHASE_TRACE define; product semantics,
# allocation order and subsystem initialization are untouched. Without the
# define every MEM_PHASE_* macro expands to (void)0.
export GROOVEPUTER_BUILD_EXTRA_CPP_FLAGS="${GROOVEPUTER_BUILD_EXTRA_CPP_FLAGS:-} -DGROOVEPUTER_MEMORY_PHASE_TRACE"
export BUILD_PATH="${BUILD_PATH:-${PROJECT_ROOT}/build/cardputer-memory-phase-trace}"
bash "${SCRIPT_DIR}/build.sh" "$@"
bash "${SCRIPT_DIR}/check_cardputer_dram_budget.sh" \
  "${BUILD_PATH}/GroovePuter.ino.elf"
