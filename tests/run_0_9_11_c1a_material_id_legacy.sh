#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/c1a-material-id-legacy"
CXX="${CXX:-g++}"
mkdir -p "${BUILD_DIR}"
source "${ROOT_DIR}/tests/hgs_song_state_sources.sh"
cat > "${BUILD_DIR}/pattern_paging_scratch.cpp" <<'CPP'
#include "scenes.h"
Scene& sceneTransactionScratch() { static Scene scratch{}; return scratch; }
CPP
"${CXX}" -std=c++17 -O2 -Wall -Wextra -Werror -Wno-c++20-extensions \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_0_9_11_c1a_material_id_legacy.cpp" \
  "${ROOT_DIR}/src/audio/pattern_paging.cpp" \
  "${ROOT_DIR}/src/audio/pattern_project_cleanup.cpp" \
  "${HGS_SONG_STATE_SOURCES[@]}" \
  "${ROOT_DIR}/src/generation/generation_context.cpp" \
  "${BUILD_DIR}/pattern_paging_scratch.cpp" \
  -o "${BUILD_DIR}/test_c1a_material_id_legacy"
"${BUILD_DIR}/test_c1a_material_id_legacy"
