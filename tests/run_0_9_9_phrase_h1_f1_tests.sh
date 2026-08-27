#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

BASE_SHA="74456bcfec0fc74138ec0d8c652dde642c7e16b6"
CXXFLAGS=(-std=c++17 -Wall -Wextra -Werror -I.)
OWNER_SRC=(
  src/generation/generation_context.cpp
  src/generation/roles/chord_progression.cpp
)
FOCUSED=tests/test_0_9_9_phrase_h1_f1_global_source.cpp
CORPUS=tests/test_0_9_9_phrase_h1_f1_plan_compatibility.cpp
TMP="${TMPDIR:-/tmp}/grooveputer_phrase_h1_f1"
rm -rf "$TMP"
mkdir -p "$TMP"

changed_src="$(git diff --name-only "$BASE_SHA" -- src/)"
expected=$'src/generation/roles/chord_progression.cpp\nsrc/generation/roles/chord_progression.h'
if [[ "$changed_src" != "$expected" ]]; then
  printf 'H1-F1 production delta escaped owner:\n%s\n' "$changed_src" >&2
  exit 1
fi
printf '%s\n' 'H1-F1 production delta firewall: OK'

g++ "${CXXFLAGS[@]}" "${OWNER_SRC[@]}" "$FOCUSED" -o "$TMP/h1-f1-gcc"
"$TMP/h1-f1-gcc" | tee "$TMP/h1-f1-gcc.out"
"$TMP/h1-f1-gcc" > "$TMP/h1-f1-gcc-repeat.out"
diff -u "$TMP/h1-f1-gcc.out" "$TMP/h1-f1-gcc-repeat.out"

if command -v clang++ >/dev/null 2>&1; then
  clang++ "${CXXFLAGS[@]}" "${OWNER_SRC[@]}" "$FOCUSED" -o "$TMP/h1-f1-clang"
  "$TMP/h1-f1-clang" > "$TMP/h1-f1-clang.out"
  diff -u "$TMP/h1-f1-gcc.out" "$TMP/h1-f1-clang.out"
fi

g++ "${CXXFLAGS[@]}" -fsanitize=address -fno-omit-frame-pointer \
  "${OWNER_SRC[@]}" "$FOCUSED" -o "$TMP/h1-f1-asan"
ASAN_OPTIONS=detect_leaks=1 "$TMP/h1-f1-asan" > "$TMP/h1-f1-asan.out"
diff -u "$TMP/h1-f1-gcc.out" "$TMP/h1-f1-asan.out"

g++ "${CXXFLAGS[@]}" -fsanitize=undefined -fno-sanitize-recover=undefined \
  "${OWNER_SRC[@]}" "$FOCUSED" -o "$TMP/h1-f1-ubsan"
"$TMP/h1-f1-ubsan" > "$TMP/h1-f1-ubsan.out"
diff -u "$TMP/h1-f1-gcc.out" "$TMP/h1-f1-ubsan.out"

# Direct frozen-H1 semantic corpus comparison for the existing public plan API.
g++ "${CXXFLAGS[@]}" "${OWNER_SRC[@]}" "$CORPUS" -o "$TMP/current-corpus"
"$TMP/current-corpus" > "$TMP/current-corpus.out"

FROZEN="$TMP/frozen-h1"
git worktree add --quiet --detach "$FROZEN" "$BASE_SHA"
cleanup() {
  git worktree remove --force "$FROZEN" >/dev/null 2>&1 || true
}
trap cleanup EXIT
cp "$CORPUS" "$FROZEN/tests/test_0_9_9_phrase_h1_f1_plan_compatibility.cpp"
(
  cd "$FROZEN"
  g++ "${CXXFLAGS[@]}" \
    src/generation/generation_context.cpp \
    src/generation/roles/chord_progression.cpp \
    tests/test_0_9_9_phrase_h1_f1_plan_compatibility.cpp \
    -o "$TMP/frozen-corpus"
)
"$TMP/frozen-corpus" > "$TMP/frozen-corpus.out"
diff -u "$TMP/frozen-corpus.out" "$TMP/current-corpus.out"

printf '%s\n' 'H1-F1 frozen H1 ChordProgressionPlan corpus parity: OK'
printf '%s\n' '0.9.9-PHRASE-H1-F1 focused GCC/repeat/Clang/ASan/UBSan gate: OK'
