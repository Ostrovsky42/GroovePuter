#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests"
CXX="${CXX:-g++}"

mkdir -p "${BUILD_DIR}"

# B — stable path identity and legacy collision behavior.
bash "${ROOT_DIR}/tests/run_sampler_ref_tests.sh"

# C — registry binding and boot ordering.
bash "${ROOT_DIR}/tests/run_sampler_registry_boot_tests.sh"

# D — stable Scene persistence / bounded streaming migration.
bash "${ROOT_DIR}/tests/run_sampler_persistence_ownership_tests.sh"

# E/F/G — final page ownership, realtime boundary and release boundary.
python3 "${ROOT_DIR}/tests/test_sampler_safe_preload_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_sampler_page_navigation_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_ui_session_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_sampler_recovery_0_9_3_source_regressions.py"

# Existing SamplerVoice lifetime/reverse regression stays executable as a
# behavior test rather than being replaced by a source assertion.
"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_sampler_voice.cpp" \
  "${ROOT_DIR}/src/sampler/sampler_voice.cpp" \
  -o "${BUILD_DIR}/test_sampler_voice_0_9_3"

"${BUILD_DIR}/test_sampler_voice_0_9_3"

echo "0.9.3 sampler recovery aggregate passed"
