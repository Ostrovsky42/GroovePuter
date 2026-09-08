#!/usr/bin/env bash
set -euo pipefail

# MEMORY-R1 FS1B: rebuild exactly the FatFs archive used by the Cardputer ADV
# Arduino core, changing only CONFIG_FATFS_USE_DYN_BUFFERS 0 -> 1.
# The shared Arduino installation is never modified.

IDF="${IDF:-$HOME/esp-idf}"
COMMIT="858a988d6eb90f54661abe282c523879c6ad0116"
AL="${ARDUINO_IDF_LIBS:-$HOME/.arduino15/packages/m5stack/tools/esp32-arduino-libs/idf-release_v5.4-858a988d-v1/esp32s3}"
TOOLBIN="${ARDUINO_XTENSA_TOOLBIN:-$HOME/.arduino15/packages/m5stack/tools/esp-x32/2411/bin}"
CC="${CC:-$TOOLBIN/xtensa-esp32s3-elf-gcc}"
AR="${AR:-$TOOLBIN/xtensa-esp32s3-elf-ar}"
NM="${NM:-$TOOLBIN/xtensa-esp32s3-elf-nm}"
OUT="${OUT:-/tmp/fatfs-build}"
STOCK="$AL/lib/libfatfs.a"
SDKCONFIG="$AL/dio_qspi/include/sdkconfig.h"

for path in "$IDF/.git" "$AL" "$STOCK" "$SDKCONFIG" "$CC" "$AR" "$NM"; do
  if [[ ! -e "$path" ]]; then
    echo "FS1B prerequisite missing: $path" >&2
    exit 2
  fi
done

git -C "$IDF" cat-file -e "${COMMIT}^{commit}"

rm -rf "$OUT"
mkdir -p "$OUT/src" "$OUT/obj" "$OUT/inc"
git -C "$IDF" archive "$COMMIT" components/fatfs | tar -x -C "$OUT/src"
F="$OUT/src/components/fatfs"
cp "$SDKCONFIG" "$OUT/inc/sdkconfig.h"

# Assert the production configuration before changing it. This checkpoint must
# not obtain memory by changing wear-levelling sector size, private file caches,
# or any unrelated SDK option.
python3 - "$OUT/inc/sdkconfig.h" <<'PY'
from pathlib import Path
import re
import sys

path = Path(sys.argv[1])
text = path.read_text(encoding="utf-8")

def value(name: str) -> int:
    m = re.search(rf"^#define\s+{re.escape(name)}\s+([0-9]+)\s*$", text, re.M)
    if not m:
        raise SystemExit(f"{path}: missing numeric {name}")
    return int(m.group(1))

expected = {
    "CONFIG_FATFS_USE_DYN_BUFFERS": 0,
    "CONFIG_FATFS_PER_FILE_CACHE": 1,
    "CONFIG_WL_SECTOR_SIZE": 4096,
}
for name, want in expected.items():
    got = value(name)
    if got != want:
        raise SystemExit(f"{path}: expected {name}={want}, got {got}")

new_text, count = re.subn(
    r"^#define\s+CONFIG_FATFS_USE_DYN_BUFFERS\s+0\s*$",
    "#define CONFIG_FATFS_USE_DYN_BUFFERS 1",
    text,
    count=1,
    flags=re.M,
)
if count != 1:
    raise SystemExit(f"{path}: dynamic-buffer config replacement count={count}")

# Re-read guards from the candidate text as a second assertion.
for name, want in (
    ("CONFIG_FATFS_USE_DYN_BUFFERS", 1),
    ("CONFIG_FATFS_PER_FILE_CACHE", 1),
    ("CONFIG_WL_SECTOR_SIZE", 4096),
):
    m = re.search(rf"^#define\s+{re.escape(name)}\s+([0-9]+)\s*$", new_text, re.M)
    if not m or int(m.group(1)) != want:
        raise SystemExit(f"{path}: candidate guard failed for {name}={want}")

path.write_text(new_text, encoding="utf-8")
PY

CFLAGS="$(cat "$AL/flags/c_flags")"
DEFS="$(cat "$AL/flags/defines")"
INC="$(cat "$AL/flags/includes")"
LOCAL="-I$F/diskio -I$F/src -I$F/vfs"

# Exactly the member set of the stock libfatfs.a at the pinned framework.
SOURCES=(
  diskio/diskio.c
  diskio/diskio_rawflash.c
  diskio/diskio_wl.c
  src/ff.c
  src/ffunicode.c
  port/freertos/ffsystem.c
  diskio/diskio_sdmmc.c
  vfs/vfs_fat.c
  vfs/vfs_fat_sdmmc.c
  vfs/vfs_fat_spiflash.c
)
OBJECTS=()
for source in "${SOURCES[@]}"; do
  object="$OUT/obj/$(basename "$source").obj"
  # c_flags/defines/includes are response-file-style strings shipped with the
  # Arduino core; intentional word splitting reproduces the core build flags.
  # shellcheck disable=SC2086
  "$CC" $CFLAGS $DEFS -iprefix "$AL/include/" $INC -I"$OUT/inc" $LOCAL \
    -c "$F/$source" -o "$object"
  OBJECTS+=("$object")
done

"$AR" rcs "$OUT/libfatfs.a" "${OBJECTS[@]}"
CANDIDATE="$OUT/libfatfs.a"

normalize_nm() {
  "$NM" "$@" | sed -E 's#^.*/##' | LC_ALL=C sort
}

# ABI/rebuild-radius gates. Member names and symbol contracts must be identical;
# only implementation/layout inside the one FatFs archive may differ.
diff -u \
  <("$AR" t "$STOCK" | LC_ALL=C sort) \
  <("$AR" t "$CANDIDATE" | LC_ALL=C sort)

diff -u \
  <(normalize_nm -g --defined-only "$STOCK") \
  <(normalize_nm -g --defined-only "$CANDIDATE")

diff -u \
  <(normalize_nm -u "$STOCK") \
  <(normalize_nm -u "$CANDIDATE")

stock_ff_memalloc="$("$NM" -u "$STOCK" | grep -c '[[:space:]]ff_memalloc$' || true)"
candidate_ff_memalloc="$("$NM" -u "$CANDIDATE" | grep -c '[[:space:]]ff_memalloc$' || true)"
if [[ "$stock_ff_memalloc" -ne 0 || "$candidate_ff_memalloc" -lt 1 ]]; then
  echo "FS1B ff_memalloc gate failed: stock=$stock_ff_memalloc candidate=$candidate_ff_memalloc" >&2
  exit 3
fi

printf 'FS1B source commit: %s\n' "$COMMIT"
printf 'FS1B stock archive: %s\n' "$STOCK"
printf 'FS1B candidate: %s\n' "$CANDIDATE"
printf 'FS1B ff_memalloc undefined refs: stock=%s candidate=%s\n' \
  "$stock_ff_memalloc" "$candidate_ff_memalloc"
sha256sum "$STOCK" "$CANDIDATE"
