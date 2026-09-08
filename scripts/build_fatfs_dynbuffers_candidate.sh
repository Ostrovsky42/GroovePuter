#!/usr/bin/env bash
set -euo pipefail

# MEMORY-R1 FS1B: rebuild exactly the FatFs archive used by the Cardputer ADV
# Arduino core, enabling only CONFIG_FATFS_USE_DYN_BUFFERS.
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

# Assert the production configuration before changing it. In the shipped
# Arduino sdkconfig.h a disabled bool may be omitted entirely rather than
# materialized as `#define ... 0`; both representations mean disabled.
python3 - "$OUT/inc/sdkconfig.h" <<'PY'
from pathlib import Path
import re
import sys

path = Path(sys.argv[1])
text = path.read_text(encoding="utf-8")


def numeric_value(name: str) -> int:
    m = re.search(rf"^#define\s+{re.escape(name)}\s+([0-9]+)\s*$", text, re.M)
    if not m:
        raise SystemExit(f"{path}: missing numeric {name}")
    return int(m.group(1))


for name, want in (
    ("CONFIG_FATFS_PER_FILE_CACHE", 1),
    ("CONFIG_WL_SECTOR_SIZE", 4096),
):
    got = numeric_value(name)
    if got != want:
        raise SystemExit(f"{path}: expected {name}={want}, got {got}")

name = "CONFIG_FATFS_USE_DYN_BUFFERS"
dyn = re.search(rf"^#define\s+{name}(?:\s+([^\s/]+))?\s*$", text, re.M)
if dyn:
    raw = dyn.group(1)
    if raw is None:
        raise SystemExit(f"{path}: {name} is already defined without a numeric false value")
    try:
        current = int(raw, 0)
    except ValueError as exc:
        raise SystemExit(f"{path}: unsupported {name} value {raw!r}") from exc
    if current != 0:
        raise SystemExit(f"{path}: expected {name} disabled, got {current}")
    new_text, count = re.subn(
        rf"^#define\s+{name}\s+0\s*$",
        f"#define {name} 1",
        text,
        count=1,
        flags=re.M,
    )
    if count != 1:
        raise SystemExit(f"{path}: disabled {name} replacement count={count}")
else:
    new_text = text
    if not new_text.endswith("\n"):
        new_text += "\n"
    new_text += f"#define {name} 1\n"

# Re-read all three guards from the candidate text.
for guard, want in (
    ("CONFIG_FATFS_USE_DYN_BUFFERS", 1),
    ("CONFIG_FATFS_PER_FILE_CACHE", 1),
    ("CONFIG_WL_SECTOR_SIZE", 4096),
):
    m = re.search(rf"^#define\s+{re.escape(guard)}\s+([0-9]+)\s*$", new_text, re.M)
    if not m or int(m.group(1)) != want:
        raise SystemExit(f"{path}: candidate guard failed for {guard}={want}")

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

symbol_names() {
  "$NM" "$@" | awk 'NF >= 2 { print $NF }' | LC_ALL=C sort -u
}

# ABI/rebuild-radius gates. The archive member set and all exported definitions
# remain identical. Dynamic buffering is allowed to add only the allocator
# requirements supplied by IDF's FatFs system port.
diff -u \
  <("$AR" t "$STOCK" | LC_ALL=C sort) \
  <("$AR" t "$CANDIDATE" | LC_ALL=C sort)

diff -u \
  <(symbol_names -g --defined-only "$STOCK") \
  <(symbol_names -g --defined-only "$CANDIDATE")

symbol_names -u "$STOCK" > "$OUT/stock.undefined"
symbol_names -u "$CANDIDATE" > "$OUT/candidate.undefined"
python3 - "$OUT/stock.undefined" "$OUT/candidate.undefined" <<'PY'
from pathlib import Path
import sys

stock = set(Path(sys.argv[1]).read_text(encoding="utf-8").splitlines())
candidate = set(Path(sys.argv[2]).read_text(encoding="utf-8").splitlines())
added = candidate - stock
removed = stock - candidate
allowed_added = {"ff_memalloc", "ff_memfree"}
if removed:
    raise SystemExit(f"FS1B undefined-symbol gate: removed={sorted(removed)}")
if not added <= allowed_added:
    raise SystemExit(f"FS1B undefined-symbol gate: unexpected added={sorted(added)}")
if "ff_memalloc" not in candidate:
    raise SystemExit("FS1B undefined-symbol gate: candidate does not require ff_memalloc")
print(f"FS1B undefined-symbol additions: {sorted(added)}")
PY

printf 'FS1B source commit: %s\n' "$COMMIT"
printf 'FS1B stock archive: %s\n' "$STOCK"
printf 'FS1B candidate: %s\n' "$CANDIDATE"
sha256sum "$STOCK" "$CANDIDATE"
