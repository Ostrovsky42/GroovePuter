#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build/host-tests/m-working-storage"
mkdir -p "$BUILD"
CXX="${CXX:-g++}"

cd "$ROOT"
"$CXX" -std=c++17 -O2 -Wall -Wextra -Werror -Wno-c++20-extensions -I. \
  tests/test_0_9_11_m_working_storage.cpp \
  -o "$BUILD/test_m_working_storage"
"$BUILD/test_m_working_storage"
