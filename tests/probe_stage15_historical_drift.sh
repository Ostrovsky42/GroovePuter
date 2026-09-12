#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

BASE="cea42fcde945eed651c5cb4413e9eb6616f3d407"
TARGET="4647a0115e752d1affa5b1a53ebd8f38bf3a025b"
TMP="${TMPDIR:-/tmp}/grooveputer-stage15-history-$$"
BASELINE="$TMP/frozen.tsv"
ACTUAL="$TMP/actual.tsv"
WORKTREE="$TMP/worktree"
mkdir -p "$TMP"

cleanup() {
  git worktree remove --force "$WORKTREE" >/dev/null 2>&1 || true
  rm -rf "$TMP"
}
trap cleanup EXIT

git cat-file -e "${BASE}^{commit}"
git cat-file -e "${TARGET}^{commit}"
if [[ "$(git merge-base "$BASE" "$TARGET")" != "$BASE" ]]; then
  echo "PROBE_INVALID: baseline is not ancestor of target" >&2
  exit 2
fi

git show "${BASE}:tests/data/stage15_tonal_legacy_baseline.tsv" > "$BASELINE"

probe_sha() {
  local sha="$1"
  git worktree add --detach "$WORKTREE" "$sha" >/dev/null
  if ! (cd "$WORKTREE" && bash tests/run_stage15_tonal_baseline_dump.sh > "$ACTUAL"); then
    echo "BUILD_FAIL=$sha"
    git worktree remove --force "$WORKTREE" >/dev/null
    return 2
  fi
  if cmp -s "$BASELINE" "$ACTUAL"; then
    echo "FROZEN_MATCH=$sha"
    git worktree remove --force "$WORKTREE" >/dev/null
    return 0
  fi
  echo "FIRST_DIFF=$sha"
  diff -u "$BASELINE" "$ACTUAL" | head -n 80 || true
  git worktree remove --force "$WORKTREE" >/dev/null
  return 1
}

# Prove the captured baseline is self-consistent before using it as the oracle.
if ! probe_sha "$BASE"; then
  echo "BASELINE_SELF_CHECK_FAIL=$BASE" >&2
  exit 3
fi

echo "BASELINE_SELF_CHECK=GREEN"
last_match="$BASE"
first_build_fail=""

while read -r sha; do
  [[ -n "$sha" ]] || continue
  set +e
  probe_sha "$sha"
  status=$?
  set -e
  if [[ $status -eq 0 ]]; then
    last_match="$sha"
    continue
  fi
  if [[ $status -eq 1 ]]; then
    echo "STAGE15_LAST_FROZEN_MATCH=$last_match"
    [[ -z "$first_build_fail" ]] || echo "STAGE15_FIRST_BUILD_GAP=$first_build_fail"
    echo "STAGE15_FIRST_BUILDABLE_DRIFT=$sha"
    exit 0
  fi
  if [[ $status -eq 2 ]]; then
    [[ -n "$first_build_fail" ]] || first_build_fail="$sha"
    continue
  fi
  echo "STAGE15_PROBE_INCONCLUSIVE_AT=$sha" >&2
  exit "$status"
done < <(git rev-list --reverse --ancestry-path "${BASE}..${TARGET}")

if [[ -n "$first_build_fail" ]]; then
  echo "STAGE15_LAST_FROZEN_MATCH=$last_match"
  echo "STAGE15_FIRST_BUILD_GAP=$first_build_fail"
  echo "STAGE15_BUILDABLE_TARGET_MATCH=$TARGET"
fi
echo "NO_BUILDABLE_DRIFT_THROUGH=$TARGET"
