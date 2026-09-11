#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build/host-tests/a2b_promotion_transaction"
mkdir -p "$BUILD"
CXX="${CXX:-g++}"

cd "$ROOT"
"$CXX" -std=c++17 -O2 -Wall -Wextra -Werror -Wno-c++20-extensions -I. \
  tests/test_0_9_11_a2b_promotion_transaction.cpp \
  -o "$BUILD/test_a2b_promotion_transaction"

"$BUILD/test_a2b_promotion_transaction"
