#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/grooveputer-repro-test.XXXXXX")"
cleanup() {
  rm -rf "$WORK"
}
trap cleanup EXIT

export FQBN='m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=default,UploadMode=cdc'
export BUILD_PATH="${BUILD_PATH:-$ROOT/build/cardputer-adv-dynbuffers}"
export SOURCE_DATE_EPOCH="$(git -C "$ROOT" log -1 --format=%ct HEAD)"

# Use the same absolute build path for both clean builds. Arduino's link result
# can depend on whether it links freshly discovered library objects or cached
# library archives; the release workflow uses this exact stable path as well.
for run in first second; do
  if ! bash "$ROOT/scripts/build_cardputer_dynbuffers.sh" --warnings all \
      > "$WORK/$run.log" 2>&1; then
    tail -n 40 "$WORK/$run.log" >&2
    exit 1
  fi
  cp "$BUILD_PATH/.arduino-build/GroovePuter.ino.bin" "$WORK/$run.bin"
  sha256sum "$WORK/$run.bin"
done

if ! cmp -s "$WORK/first.bin" "$WORK/second.bin"; then
  echo "release firmware build is not reproducible for the same source/profile" >&2
  echo "comparison artifacts preserved at: $WORK" >&2
  trap - EXIT
  exit 1
fi

echo "Release firmware reproducibility: PASS"
