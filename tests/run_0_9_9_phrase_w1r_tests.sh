#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

CXXFLAGS=(-std=c++17 -Wall -Wextra -Werror -I.)
OWNER_SRC=(
  src/generation/generation_context.cpp
  src/generation/roles/chord_progression.cpp
)
W1_SRC=tests/test_0_9_9_phrase_w1_harmonic_when_owner.cpp
COMPAT_SRC=tests/test_0_9_9_phrase_w1r_h1_f1_compat.cpp
TMP="${TMPDIR:-/tmp}/grooveputer_phrase_w1r"
WORKTREES="$TMP/worktrees"
F08_SHA=cfb1f9a8e214cfcb823a5e75445f26356b55bed6
H1_SHA=74456bcfec0fc74138ec0d8c652dde642c7e16b6
OLD_W1_SHA=34912cd050c04727c13533575b2cf999816e0549

rm -rf "$TMP"
mkdir -p "$WORKTREES"
cleanup() {
  git worktree remove --force "$WORKTREES/f08" >/dev/null 2>&1 || true
  git worktree remove --force "$WORKTREES/h1" >/dev/null 2>&1 || true
  git worktree remove --force "$WORKTREES/old-w1" >/dev/null 2>&1 || true
}
trap cleanup EXIT

python3 tests/test_0_9_9_phrase_w1r_source_guard.py

compile_and_capture() {
  local compiler="$1"
  local source="$2"
  local binary="$3"
  local output="$4"
  shift 4
  "$compiler" "${CXXFLAGS[@]}" "$@" "${OWNER_SRC[@]}" "$source" -o "$binary"
  "$binary" > "$output"
}

compile_and_capture g++ "$W1_SRC" "$TMP/w1-gcc" "$TMP/w1-gcc.out"
"$TMP/w1-gcc" > "$TMP/w1-gcc-repeat.out"
diff -u "$TMP/w1-gcc.out" "$TMP/w1-gcc-repeat.out"

compile_and_capture g++ "$COMPAT_SRC" "$TMP/compat-gcc" "$TMP/compat-gcc.out"
"$TMP/compat-gcc" > "$TMP/compat-gcc-repeat.out"
diff -u "$TMP/compat-gcc.out" "$TMP/compat-gcc-repeat.out"
cat "$TMP/w1-gcc.out"
cat "$TMP/compat-gcc.out"

if command -v clang++ >/dev/null 2>&1; then
  compile_and_capture clang++ "$W1_SRC" "$TMP/w1-clang" "$TMP/w1-clang.out"
  diff -u "$TMP/w1-gcc.out" "$TMP/w1-clang.out"
  compile_and_capture clang++ "$COMPAT_SRC" "$TMP/compat-clang" "$TMP/compat-clang.out"
  diff -u "$TMP/compat-gcc.out" "$TMP/compat-clang.out"
fi

compile_and_capture g++ "$W1_SRC" "$TMP/w1-asan" "$TMP/w1-asan.out" \
  -fsanitize=address -fno-omit-frame-pointer
ASAN_OPTIONS=detect_leaks=1 "$TMP/w1-asan" > "$TMP/w1-asan.out"
diff -u "$TMP/w1-gcc.out" "$TMP/w1-asan.out"
compile_and_capture g++ "$COMPAT_SRC" "$TMP/compat-asan" "$TMP/compat-asan.out" \
  -fsanitize=address -fno-omit-frame-pointer
ASAN_OPTIONS=detect_leaks=1 "$TMP/compat-asan" > "$TMP/compat-asan.out"
diff -u "$TMP/compat-gcc.out" "$TMP/compat-asan.out"

compile_and_capture g++ "$W1_SRC" "$TMP/w1-ubsan" "$TMP/w1-ubsan.out" \
  -fsanitize=undefined -fno-sanitize-recover=undefined
"$TMP/w1-ubsan" > "$TMP/w1-ubsan.out"
diff -u "$TMP/w1-gcc.out" "$TMP/w1-ubsan.out"
compile_and_capture g++ "$COMPAT_SRC" "$TMP/compat-ubsan" "$TMP/compat-ubsan.out" \
  -fsanitize=undefined -fno-sanitize-recover=undefined
"$TMP/compat-ubsan" > "$TMP/compat-ubsan.out"
diff -u "$TMP/compat-gcc.out" "$TMP/compat-ubsan.out"

# Accepted F08 authority, frozen H1 baseline, old W1 final, and W1R are compared
# without regenerating or approving any tracked golden.
git worktree add --quiet --detach "$WORKTREES/f08" "$F08_SHA"
git worktree add --quiet --detach "$WORKTREES/h1" "$H1_SHA"
git worktree add --quiet --detach "$WORKTREES/old-w1" "$OLD_W1_SHA"

bash "$WORKTREES/f08/tests/run_stage15_tonal_baseline_dump.sh" --tonal > "$TMP/f08.tsv"
bash "$WORKTREES/h1/tests/run_stage15_tonal_baseline_dump.sh" --tonal > "$TMP/h1.tsv"
bash "$WORKTREES/old-w1/tests/run_stage15_tonal_baseline_dump.sh" --tonal > "$TMP/old-w1.tsv"
bash tests/run_stage15_tonal_baseline_dump.sh --tonal > "$TMP/w1r.tsv"

python3 tests/test_0_9_9_phrase_w1_corpus_compare.py \
  "$WORKTREES/f08/tests/data/stage15_tonal_enabled_baseline.tsv" \
  "$TMP/f08.tsv" \
  "$TMP/h1.tsv" \
  "$TMP/w1r.tsv"

diff -u "$TMP/old-w1.tsv" "$TMP/w1r.tsv"

printf '%s\n' 'W1R old-W1 tonal replay parity=EXACT'
printf '%s\n' 'W1R one-bar F08 compatibility fingerprint=static:0001/1;moving:0101/2'
printf '%s\n' 'W1R F08.1 imported=NO'
printf '%s\n' '0.9.9-PHRASE-W1R focused GCC/repeat/Clang/ASan/UBSan gate: OK (DECISION_B reproduced)'
