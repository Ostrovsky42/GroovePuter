#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

ORACLE="cea42fcde945eed651c5cb4413e9eb6616f3d407"
ANCHOR="76ec22d52bbccf46c6228f287f410b01db90f41c"
TARGET="3697ec38cc7f5fe2aa87bec5b60c3f25a504bc03"
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

for sha in "$ORACLE" "$ANCHOR" "$TARGET"; do
  git cat-file -e "${sha}^{commit}"
done
if [[ "$(git merge-base "$ORACLE" "$ANCHOR")" != "$ORACLE" ]]; then
  echo "PROBE_INVALID: oracle is not ancestor of anchor" >&2
  exit 2
fi
if [[ "$(git merge-base "$ANCHOR" "$TARGET")" != "$ANCHOR" ]]; then
  echo "PROBE_INVALID: anchor is not ancestor of target" >&2
  exit 2
fi

git show "${ORACLE}:tests/data/stage15_tonal_legacy_baseline.tsv" > "$BASELINE"

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
  echo "FROZEN_DIFF=$sha"
  diff -u "$BASELINE" "$ACTUAL" | head -n 80 || true
  git worktree remove --force "$WORKTREE" >/dev/null
  return 1
}

# ANCHOR is the latest candidate already proved byte-identical to the original
# frozen oracle. Re-run it so every candidate verdict is self-contained.
if ! probe_sha "$ANCHOR"; then
  echo "ANCHOR_NOT_FROZEN=$ANCHOR" >&2
  exit 3
fi
echo "STAGE15_FROZEN_ANCHOR=$ANCHOR"

set +e
probe_sha "$TARGET"
status=$?
set -e
case "$status" in
  0)
    echo "STAGE15_CANDIDATE_MATCH=$TARGET"
    echo "NO_DRIFT_THROUGH_CANDIDATE=$TARGET"
    ;;
  1)
    echo "STAGE15_LAST_PROVEN_FROZEN=$ANCHOR"
    echo "STAGE15_FIRST_KNOWN_DRIFT_CANDIDATE=$TARGET"
    echo "STAGE15_BISECT_REQUIRED=YES"
    ;;
  2)
    echo "STAGE15_CANDIDATE_BUILD_GAP=$TARGET" >&2
    exit 2
    ;;
  *)
    echo "STAGE15_PROBE_INCONCLUSIVE_AT=$TARGET" >&2
    exit "$status"
    ;;
esac
