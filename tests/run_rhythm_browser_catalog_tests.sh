#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build/host-tests"
mkdir -p "${BUILD}"
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -Wno-c++20-extensions \
  ${RHYTHM_BROWSER_TEST_FLAGS:-} -I"${ROOT}" \
  "${ROOT}/tests/test_rhythm_browser_catalog.cpp" \
  "${ROOT}/src/generation/rhythm_browser/rhythm_browser_catalog.cpp" \
  "${ROOT}/src/dsp/atlas_runtime.cpp" \
  "${ROOT}/src/generation/generation_context.cpp" \
  "${ROOT}/src/generation/composition/rhythm_selection.cpp" \
  "${ROOT}/src/generation/rhythm/rhythm_catalog.cpp" \
  "${ROOT}/src/generation/rhythm/relationship_resolver.cpp" \
  "${ROOT}/src/generation/rhythm/rhythm_realizer.cpp" \
  "${ROOT}/src/generation/rhythm/reference_vocabulary.cpp" \
  "${ROOT}/src/generation/materialization/pattern_materializer.cpp" \
  -o "${BUILD}/test_rhythm_browser_catalog"
"${BUILD}/test_rhythm_browser_catalog"
