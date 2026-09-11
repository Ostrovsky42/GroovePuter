#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/material-id-project-persistence"
CXX="${CXX:-g++}"

mkdir -p "${BUILD_DIR}"

cat > "${BUILD_DIR}/pattern_paging_scratch.cpp" <<'CPP'
#include "scenes.h"

Scene& sceneTransactionScratch() {
  static Scene scratch{};
  return scratch;
}
CPP

"${CXX}" \
  -std=c++17 -O2 -Wall -Wextra -Werror -Wno-c++20-extensions \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_0_9_11_material_id_project_persistence.cpp" \
  "${ROOT_DIR}/src/audio/pattern_paging.cpp" \
  "${ROOT_DIR}/src/audio/pattern_project_cleanup.cpp" \
  "${BUILD_DIR}/pattern_paging_scratch.cpp" \
  -o "${BUILD_DIR}/test_material_id_project_persistence"

"${BUILD_DIR}/test_material_id_project_persistence"
