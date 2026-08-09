#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build/stage7a"
mkdir -p "$BUILD"

CXX_BIN="${CXX:-g++}"
COMMON=(
  "$ROOT/tests/test_stage7a_audition.cpp"
  "$ROOT/src/generation/generation_context.cpp"
  "$ROOT/src/generation/rhythm/rhythm_catalog.cpp"
  "$ROOT/src/generation/rhythm/relationship_resolver.cpp"
  "$ROOT/src/generation/rhythm/rhythm_realizer.cpp"
  "$ROOT/src/generation/audition_stage7/stage7a_catalog.cpp"
  "$ROOT/src/generation/audition_stage7/stage7a_session.cpp"
)

"$CXX_BIN" -std=c++17 -O2 -Wall -Wextra -Werror -Wno-c++20-extensions \
  -I"$ROOT" "${COMMON[@]}" -o "$BUILD/stage7a-gcc"
"$BUILD/stage7a-gcc"

if command -v clang++ >/dev/null 2>&1; then
  clang++ -std=c++17 -O2 -Wall -Wextra -Werror -Wno-c++20-extensions \
    -I"$ROOT" "${COMMON[@]}" -o "$BUILD/stage7a-clang"
  "$BUILD/stage7a-clang"
fi

"$CXX_BIN" -std=c++17 -O1 -g -Wall -Wextra -Werror -Wno-c++20-extensions \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"$ROOT" "${COMMON[@]}" -o "$BUILD/stage7a-sanitize"
"$BUILD/stage7a-sanitize"

python3 "$ROOT/tests/test_stage7a_source_regressions.py"

echo "Stage 7A five-candidate audition tests: OK"
