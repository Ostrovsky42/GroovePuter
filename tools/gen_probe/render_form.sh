#!/usr/bin/env bash
# Build the host listening renderer and keep its project files out of the repo.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="${1:-$ROOT/build/form_audition}"
mkdir -p "$OUT" "$ROOT/build/gen_probe"
OUT="$(cd "$OUT" && pwd)"
BIN="$ROOT/build/gen_probe/formrender"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/form-render.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT
"$ROOT/tools/gen_probe/build.sh" "$ROOT/tools/gen_probe/formrender.cpp" "$BIN"
(cd "$WORK" && "$BIN" "$OUT")
