#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
VARIANT="${1:?usage: build_target.sh baseline|candidate [benchmark|product]}"
KIND="${2:-benchmark}"
case "$VARIANT" in baseline|candidate) ;; *) exit 2;; esac
case "$KIND" in benchmark|product) ;; *) exit 2;; esac
BASE=64ae4b8bcf13fc165a04b2b79a80b7e8b3a1f71f
STAGING="$(mktemp -d /tmp/gp-dsp-a-target.XXXXXX)"
trap 'rm -rf "$STAGING"' EXIT
SOURCE="$STAGING/GroovePuter"
mkdir -p "$SOURCE"
if [[ "$VARIANT" == baseline ]]; then
  git -C "$ROOT" archive "$BASE" | tar -x -C "$SOURCE"
  SOURCE_SHA="$BASE"
else
  rsync -a --exclude .git --exclude build --exclude platform_sdl/build "$ROOT/" "$SOURCE/"
  SOURCE_SHA="$(git -C "$ROOT" rev-parse HEAD)"
fi
export BUILD_PATH="${BUILD_PATH:-$ROOT/build/dsp-a/target-$VARIANT-$KIND}"
mkdir -p "$BUILD_PATH"
printf 'variant=%s\nkind=%s\nsource=%s\nsample_rate=22050\n' "$VARIANT" "$KIND" "$SOURCE_SHA" > "$BUILD_PATH/dsp-a-provenance.txt"
git -C "$ROOT" diff --binary HEAD > "$BUILD_PATH/dsp-a-source.patch"
if [[ "$KIND" == benchmark ]]; then
  cp "$ROOT/tools/dsp_a/target_benchmark.h" "$SOURCE/src/diag/dsp_a_target_benchmark.h"
  # Recipes are identical explicit inputs for both diagnostic DSP cores.
  cp "$ROOT/src/dsp/sound_quality_presets.h" "$SOURCE/src/dsp/sound_quality_presets.h"
  cp "$ROOT/src/dsp/sound_quality_presets.cpp" "$SOURCE/src/dsp/sound_quality_presets.cpp"
  python3 - "$SOURCE/GroovePuter.ino" <<'PY'
from pathlib import Path
import sys
p=Path(sys.argv[1]);s=p.read_text()
include='#include "src/dsp/miniacid_engine.h"'
anchor='  markBootStage(51, "after MiniAcid::init");'
assert s.count(include)==1 and s.count(anchor)==1
s=s.replace(include,include+'\n#include "src/diag/dsp_a_target_benchmark.h"')
s=s.replace(anchor,anchor+'\n  GroovePuterDspA::targetBenchmark();')
p.write_text(s)
PY
fi
bash "$SOURCE/scripts/build_cardputer_dynbuffers.sh"
MEMORY_BASELINE_IMAGE_KIND="$KIND" bash "$SOURCE/scripts/report_cardputer_memory_baseline.sh" "$BUILD_PATH/GroovePuter.ino.elf"
sha256sum "$BUILD_PATH/GroovePuter.ino.bin"
