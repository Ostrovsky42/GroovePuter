#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SOURCE_COMMIT="$(git -C "$ROOT" rev-parse HEAD)"
SOURCE_DIRTY="$(git -C "$ROOT" status --porcelain | wc -l | tr -d ' ')"
BUILD_DIR="$ROOT/build/usb-acceptance"
PACKAGE_DIR="$ROOT/build/usb-acceptance-package"
FQBN_ACCEPT="m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=default,UploadMode=cdc"

rm -rf "$BUILD_DIR" "$PACKAGE_DIR"
mkdir -p "$BUILD_DIR" "$PACKAGE_DIR"

export FQBN="$FQBN_ACCEPT"
export BUILD_PATH="$BUILD_DIR"
export ARDUINO_BUILD_PATH="$BUILD_DIR/.arduino-build"
export GROOVEPUTER_BUILD_EXTRA_CPP_FLAGS="-DGROOVEPUTER_USB_ACCEPT_DIAG"

printf 'USB acceptance source: %s\n' "$SOURCE_COMMIT"
printf 'USB acceptance source dirty entries: %s\n' "$SOURCE_DIRTY"
printf 'USB acceptance FQBN: %s\n' "$FQBN"
printf 'USB acceptance flags: %s\n' "$GROOVEPUTER_BUILD_EXTRA_CPP_FLAGS"

bash "$ROOT/scripts/build_cardputer_dynbuffers.sh" --warnings all 2>&1 | tee "$PACKAGE_DIR/BUILD.log"

ELF="$BUILD_DIR/GroovePuter.ino.elf"
BIN="$BUILD_DIR/GroovePuter.ino.bin"
MAP="$ARDUINO_BUILD_PATH/GroovePuter.ino.map"

for required in "$ELF" "$BIN" "$MAP"; do
  test -f "$required" || { echo "USB acceptance artifact missing: $required" >&2; exit 3; }
done

# Prove this is the diagnostic image rather than the product image.
for marker in "ROLE %s att=%lu det=%lu on=%lu" "MEM f=%u min=%u blk=%u"               "Q held=%u drop=%lu ring=%lu" "LAT usb>disp" "LAT disp>keys"; do
  if ! strings "$ELF" | grep -Fq "$marker"; then
    echo "USB acceptance diagnostic marker missing from ELF: $marker" >&2
    exit 4
  fi
done

# Prove the accepted dynamic-FatFs link is the one being packaged.
if ! grep -Eq "(fatfs-dynbuffers|grooveputer-sdk-dynbuffers)" "$MAP"; then
  echo "USB acceptance package is not linked with the FS1B dynamic-FatFs candidate" >&2
  exit 5
fi

cp "$ELF" "$PACKAGE_DIR/"
cp "$BIN" "$PACKAGE_DIR/"
find "$BUILD_DIR" -maxdepth 1 -type f -name 'GroovePuter.ino.*.bin' -exec cp {} "$PACKAGE_DIR/" \;
cp "$MAP" "$PACKAGE_DIR/GroovePuter.ino.map"

bash "$ROOT/scripts/check_cardputer_dram_budget.sh" "$ELF" | tee "$PACKAGE_DIR/DRAM.txt"

{
  printf 'source_commit=%s\n' "$SOURCE_COMMIT"
  printf 'source_dirty_entries=%s\n' "$SOURCE_DIRTY"
  printf 'fqbn=%s\n' "$FQBN"
  printf 'extra_cpp_flags=%s\n' "$GROOVEPUTER_BUILD_EXTRA_CPP_FLAGS"
  printf 'fatfs=FS1B-dynamic-buffers\n'
  printf 'acceptance_doc=docs/releases/0.9.15-hardware-acceptance.md\n'
} > "$PACKAGE_DIR/MANIFEST.txt"

(
  cd "$PACKAGE_DIR"
  sha256sum GroovePuter.ino*.bin GroovePuter.ino.elf GroovePuter.ino.map > SHA256SUMS.txt
)

cat > "$PACKAGE_DIR/FLASH.txt" <<EOF
GroovePuter 0.9.15 USB Host hardware-acceptance image

SOURCE
  $SOURCE_COMMIT

PROFILE
  CDCOnBoot=default
  FS1B dynamic FatFs
  -DGROOVEPUTER_USB_ACCEPT_DIAG

FLASH FROM A COMPUTER-MODE / DOWNLOAD-MODE DEVICE
  FQBN='$FQBN_ACCEPT' \
  arduino-cli upload --fqbn "\$FQBN" -p /dev/ttyACM0 \
    --input-dir <extracted-artifact-directory>

After flashing, follow:
  docs/releases/0.9.15-hardware-acceptance.md

This is a diagnostic acceptance image, not the product release image.
EOF

cp "$ROOT/docs/releases/0.9.15-hardware-acceptance.md" "$PACKAGE_DIR/ACCEPTANCE.md"

echo "USB acceptance package ready: $PACKAGE_DIR"
cat "$PACKAGE_DIR/MANIFEST.txt"
cat "$PACKAGE_DIR/SHA256SUMS.txt"
