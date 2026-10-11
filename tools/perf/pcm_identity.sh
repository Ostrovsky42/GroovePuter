#!/usr/bin/env bash
# PCM identity across engine/mute configurations (render_bench pcm= hashes).
# Usage: tools/perf/pcm_identity.sh <render_bench-binary>
set -euo pipefail
BIN="$1"
for cfg in "808 TB303 1500 none" "808 TB303 1500 b" "808 TB303 1500 a" \
           "606 SID 1500 none" "909 TB303 1500 drums" "CR78 WAVEMORPH 1500 none"; do
  # shellcheck disable=SC2086
  "$BIN" $cfg 2>/dev/null | grep -o "drums=.* mute=[a-z]*\|pcm=[0-9a-f]*" | paste -sd' '
done
