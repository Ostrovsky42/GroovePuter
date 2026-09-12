#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

ORACLE="cea42fcde945eed651c5cb4413e9eb6616f3d407"
GOOD="3697ec38cc7f5fe2aa87bec5b60c3f25a504bc03"
BAD="f2261759071ad3d3f984381ba5f5c608525f8dfa"
TMP="${TMPDIR:-/tmp}/grooveputer-stage15-bisect-$$"
BASELINE="$TMP/frozen.tsv"
ACTUAL="$TMP/actual.tsv"
HELPER="$TMP/probe-one.sh"
BISECT_OUTPUT="$TMP/bisect-output.log"
mkdir -p "$TMP"

cleanup() {
  git bisect reset >/dev/null 2>&1 || true
  rm -rf "$TMP"
}
trap cleanup EXIT

for sha in "$ORACLE" "$GOOD" "$BAD"; do
  git cat-file -e "${sha}^{commit}"
done
if [[ "$(git merge-base "$ORACLE" "$GOOD")" != "$ORACLE" ]]; then
  echo "PROBE_INVALID: oracle is not ancestor of good" >&2
  exit 2
fi
if [[ "$(git merge-base "$GOOD" "$BAD")" != "$GOOD" ]]; then
  echo "PROBE_INVALID: good is not ancestor of bad" >&2
  exit 2
fi

git show "${ORACLE}:tests/data/stage15_tonal_legacy_baseline.tsv" > "$BASELINE"

cat > "$HELPER" <<EOF
#!/usr/bin/env bash
set -uo pipefail
if [[ ! -x tests/run_stage15_tonal_baseline_dump.sh && ! -f tests/run_stage15_tonal_baseline_dump.sh ]]; then
  echo "BISECT_SKIP_NO_RUNNER=\$(git rev-parse HEAD)"
  exit 125
fi
if ! bash tests/run_stage15_tonal_baseline_dump.sh > "$ACTUAL"; then
  echo "BISECT_SKIP_BUILD_FAIL=\$(git rev-parse HEAD)"
  exit 125
fi
if cmp -s "$BASELINE" "$ACTUAL"; then
  echo "BISECT_FROZEN_MATCH=\$(git rev-parse HEAD)"
  exit 0
fi
echo "BISECT_FROZEN_DIFF=\$(git rev-parse HEAD)"
diff -u "$BASELINE" "$ACTUAL" | head -n 40 || true
exit 1
EOF
chmod +x "$HELPER"

printf '%s\n' "STAGE15_BISECT_GOOD=$GOOD"
printf '%s\n' "STAGE15_BISECT_BAD=$BAD"

git bisect start "$BAD" "$GOOD"
set +e
git bisect run "$HELPER" | tee "$BISECT_OUTPUT"
bisect_status=${PIPESTATUS[0]}
set -e

if [[ $bisect_status -ne 0 ]]; then
  echo "STAGE15_BISECT_INCONCLUSIVE_STATUS=$bisect_status" >&2
  git bisect log
  exit "$bisect_status"
fi

FIRST_BAD="$(sed -nE "s/^([0-9a-f]{40}) is the first 'bad' commit$/\1/p" "$BISECT_OUTPUT" | tail -n 1)"
if [[ -z "$FIRST_BAD" ]]; then
  FIRST_BAD="$(git bisect log | sed -nE "s/^# first 'bad' commit: \[([0-9a-f]{40})\].*$/\1/p" | tail -n 1)"
fi
if [[ -z "$FIRST_BAD" ]]; then
  echo "STAGE15_FIRST_BAD_PARSE_FAILED" >&2
  git bisect log
  exit 3
fi

SUBJECT="$(git show -s --format=%s "$FIRST_BAD")"
PARENTS="$(git show -s --format=%P "$FIRST_BAD")"

# Re-check the reported first-bad SHA explicitly. git bisect run may leave HEAD
# on the final good neighbour rather than the reported culprit.
git checkout --detach "$FIRST_BAD" >/dev/null 2>&1
set +e
"$HELPER" > "$TMP/first-bad-check.log" 2>&1
first_bad_status=$?
set -e
cat "$TMP/first-bad-check.log"
if [[ $first_bad_status -ne 1 ]]; then
  echo "STAGE15_FIRST_BAD_RECHECK_INVALID_STATUS=$first_bad_status" >&2
  git bisect log
  exit 4
fi

printf '%s\n' "STAGE15_FIRST_BAD=$FIRST_BAD"
printf '%s\n' "STAGE15_FIRST_BAD_SUBJECT=$SUBJECT"
printf '%s\n' "STAGE15_FIRST_BAD_PARENTS=$PARENTS"
printf '%s\n' 'STAGE15_BASELINE_UPDATE=FORBIDDEN_PENDING_CAUSAL_REVIEW'
git bisect log
