#!/usr/bin/env bash
set -u -o pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build/host-tests/0-9-14-d0b"
BASE_SHA="d2c06a521883beba2cb56e6ae209f542aefe4609"
mkdir -p "$BUILD"

fail=0

echo "== D0-B src firewall =="
if ! git -C "$ROOT" diff --exit-code "$BASE_SHA" -- src/; then
  echo "D0-B ERROR: production src delta is forbidden during RED checkpoint"
  exit 2
fi

echo "== D0-B test-local factorization =="
if ! "${CXX:-g++}" -std=c++17 -Wall -Wextra \
    "${ROOT}/tests/test_0_9_14_d0b_semantic_contract.cpp" \
    -o "$BUILD/semantic-contract"; then
  echo "D0-B ERROR: test-local semantic contract failed to compile"
  exit 2
fi
if ! "$BUILD/semantic-contract"; then
  echo "D0-B ERROR: normative witness factorization is internally contradictory"
  exit 2
fi

echo "== D0-B legacy runtime witnesses =="
if "${CXX:-g++}" -std=c++17 -Wall -Wextra \
    -I"${ROOT}" -I"${ROOT}/platform_sdl" \
    -include "${ROOT}/platform_sdl/arduino_compat.h" \
    "${ROOT}/tests/test_0_9_14_d0b_legacy_classifier.cpp" \
    -o "$BUILD/legacy-classifier"; then
  if ! "$BUILD/legacy-classifier"; then
    fail=1
  fi
else
  echo "D0-B ERROR: legacy witness harness did not compile"
  exit 2
fi

echo "== D0-B source regressions =="
if ! python3 "${ROOT}/tests/test_0_9_14_d0b_source_regressions.py"; then
  fail=1
fi

if ! git -C "$ROOT" diff --exit-code "$BASE_SHA" -- src/; then
  echo "D0-B ERROR: src changed during test execution"
  exit 2
fi

if [[ "$fail" -ne 0 ]]; then
  echo "0.9.14 D0-B: RED CONFIRMED — semantic separation not represented by current production classifier"
  exit 1
fi

echo "0.9.14 D0-B: GREEN — normative separation represented without forbidden shortcuts"