#!/usr/bin/env bash
set -euo pipefail
VARIANT="${1:?usage: build_memory_prune_variant.sh baseline|sampler|tape|pruned|no-delay [normal|midi-only]}"
PROFILE="${2:-normal}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BASE=af4d51072e99b7cd7497c332bfc6da8877294c26
BUILD_PATH="${BUILD_PATH:-$ROOT/build/memory-prune-a-$VARIANT-$PROFILE}"
export BUILD_PATH
case "$PROFILE" in
 normal) export FQBN='m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=cdc,UploadMode=cdc' ;;
 midi-only) export FQBN='m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=default,UploadMode=cdc' ;;
 *) exit 2 ;;
esac
SOURCE="$ROOT"
SNAPSHOT=""
trap 'if [[ -n "$SNAPSHOT" ]]; then rm -rf "$SNAPSHOT"; fi' EXIT
FLAGS=''
case "$VARIANT" in
 baseline)
   SNAPSHOT="$(mktemp -d /tmp/gp-memory-prune-baseline.XXXXXX)"
   SOURCE="$SNAPSHOT/GroovePuter"
   mkdir -p "$SOURCE"
   git -C "$ROOT" archive "$BASE" | tar -x -C "$SOURCE"
   SOURCE_SHA="$BASE"
   ;;
 sampler) FLAGS='-DGROOVEPUTER_ENABLE_SAMPLER=0 -DGROOVEPUTER_ENABLE_TAPE=1 -DGROOVEPUTER_ENABLE_WAVEFORM=1' ;;
 tape) FLAGS='-DGROOVEPUTER_ENABLE_SAMPLER=0 -DGROOVEPUTER_ENABLE_TAPE=0 -DGROOVEPUTER_ENABLE_WAVEFORM=1' ;;
 pruned) FLAGS='-DGROOVEPUTER_ENABLE_SAMPLER=0 -DGROOVEPUTER_ENABLE_TAPE=0 -DGROOVEPUTER_ENABLE_WAVEFORM=0' ;;
 no-delay) FLAGS='-DGROOVEPUTER_ENABLE_SAMPLER=0 -DGROOVEPUTER_ENABLE_TAPE=0 -DGROOVEPUTER_ENABLE_WAVEFORM=0 -DGROOVEPUTER_EXPERIMENT_NO_TEMPO_DELAY=1' ;;
 *) exit 2 ;;
esac
mkdir -p "$BUILD_PATH"
SOURCE_SHA="${SOURCE_SHA:-$(git -C "$ROOT" rev-parse HEAD)}"
printf 'variant=%s\nprofile=%s\nsource=%s\nflags=%s\nfqbn=%s\n' "$VARIANT" "$PROFILE" "$SOURCE_SHA" "$FLAGS" "$FQBN" > "$BUILD_PATH/memory-prune-provenance.txt"
if [[ "$VARIANT" != baseline ]]; then git -C "$ROOT" diff --binary HEAD > "$BUILD_PATH/memory-prune-source.patch"; fi
export GROOVEPUTER_BUILD_EXTRA_CPP_FLAGS="$FLAGS"
bash "$SOURCE/scripts/build_cardputer_dynbuffers.sh"
MEMORY_BASELINE_IMAGE_KIND=product bash "$SOURCE/scripts/report_cardputer_memory_baseline.sh" "$BUILD_PATH/GroovePuter.ino.elf"
sha256sum "$BUILD_PATH/GroovePuter.ino.bin" "$BUILD_PATH/GroovePuter.ino.elf"
