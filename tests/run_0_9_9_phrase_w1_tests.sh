#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

CXXFLAGS=(-std=c++17 -Wall -Wextra -Werror -I.)
SRC=tests/test_0_9_9_phrase_w1_harmonic_when_owner.cpp
OWNER_SRC=(
  src/generation/generation_context.cpp
  src/generation/roles/chord_progression.cpp
)
TMP="${TMPDIR:-/tmp}/grooveputer_phrase_w1"
WORKTREES="$TMP/worktrees"
F08_SHA=cfb1f9a8e214cfcb823a5e75445f26356b55bed6
H1_SHA=74456bcfec0fc74138ec0d8c652dde642c7e16b6
mkdir -p "$TMP"

cleanup_worktrees() {
  git worktree remove --force "$WORKTREES/f08" >/dev/null 2>&1 || true
  git worktree remove --force "$WORKTREES/h1" >/dev/null 2>&1 || true
}
trap cleanup_worktrees EXIT
cleanup_worktrees
rm -rf "$WORKTREES"
mkdir -p "$WORKTREES"

python3 tests/test_0_9_9_phrase_w1_source_contract.py

g++ "${CXXFLAGS[@]}" "${OWNER_SRC[@]}" "$SRC" -o "$TMP/w1-gcc"
"$TMP/w1-gcc" | tee "$TMP/w1-gcc.out"
"$TMP/w1-gcc" > "$TMP/w1-gcc-repeat.out"
diff -u "$TMP/w1-gcc.out" "$TMP/w1-gcc-repeat.out"

if command -v clang++ >/dev/null 2>&1; then
  clang++ "${CXXFLAGS[@]}" "${OWNER_SRC[@]}" "$SRC" -o "$TMP/w1-clang"
  "$TMP/w1-clang" > "$TMP/w1-clang.out"
  diff -u "$TMP/w1-gcc.out" "$TMP/w1-clang.out"
fi

g++ "${CXXFLAGS[@]}" -fsanitize=address -fno-omit-frame-pointer \
  "${OWNER_SRC[@]}" "$SRC" -o "$TMP/w1-asan"
ASAN_OPTIONS=detect_leaks=1 "$TMP/w1-asan" >/dev/null

g++ "${CXXFLAGS[@]}" -fsanitize=undefined -fno-sanitize-recover=undefined \
  "${OWNER_SRC[@]}" "$SRC" -o "$TMP/w1-ubsan"
"$TMP/w1-ubsan" >/dev/null

# Replay the exact hardware-accepted F08 source and separately compare the
# current H1 corpus domain to W1. This deliberately does not replace/regenerate
# any tracked golden in the W1 branch.
git worktree add --detach "$WORKTREES/f08" "$F08_SHA" >/dev/null
git worktree add --detach "$WORKTREES/h1" "$H1_SHA" >/dev/null
bash "$WORKTREES/f08/tests/run_stage15_tonal_baseline_dump.sh" --tonal \
  > "$TMP/f08-tonal-actual.tsv"
bash "$WORKTREES/h1/tests/run_stage15_tonal_baseline_dump.sh" --tonal \
  > "$TMP/h1-tonal-actual.tsv"
bash tests/run_stage15_tonal_baseline_dump.sh --tonal \
  > "$TMP/w1-tonal-actual.tsv"
python3 tests/test_0_9_9_phrase_w1_corpus_compare.py \
  "$WORKTREES/f08/tests/data/stage15_tonal_enabled_baseline.tsv" \
  "$TMP/f08-tonal-actual.tsv" \
  "$TMP/h1-tonal-actual.tsv" \
  "$TMP/w1-tonal-actual.tsv"

printf '%s\n' 'one-bar F08 compatibility fingerprint=static:0001/1;moving:0101/2'
printf '%s\n' 'F08 accepted causal authority=256 rows; changed=93; topology=0; articulation=0; pitch=93; static=0/102'
printf '%s\n' 'F08.1 imported=NO'
printf '%s\n' '0.9.9-PHRASE-W1 harmonic WHEN owner recovery gate: OK (DECISION_B projection gap)'
