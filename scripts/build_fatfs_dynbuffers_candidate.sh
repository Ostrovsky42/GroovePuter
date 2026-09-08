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
PY

# The core's flags files contain quoted define values. Parse them with Python
# shlex rather than shell word splitting, which corrupts quoted macro values.
read_flags() {
  local file="$1"
  python3 - "$file" <<'PY'
import shlex
import sys
from pathlib import Path
for arg in shlex.split(Path(sys.argv[1]).read_text(encoding="utf-8")):
    sys.stdout.buffer.write(arg.encode() + b"\0")
PY
}

mapfile -d '' -t CFLAGS_ARGS < <(read_flags "$AL/flags/c_flags")
mapfile -d '' -t DEFS_ARGS < <(read_flags "$AL/flags/defines")
mapfile -d '' -t INC_ARGS < <(read_flags "$AL/flags/includes")

compile_common=(
  "${CFLAGS_ARGS[@]}"
  "${DEFS_ARGS[@]}"
  -iprefix "$AL/include/"
  "${INC_ARGS[@]}"
  -I"$OUT/inc"
  -I"$F/diskio" -I"$F/src" -I"$F/vfs"
)

# Reproduce the layout with compiler-owned facts. The arrays' symbol sizes equal
# sizeof(FIL), sizeof(FATFS), and FF_MAX_SS without running target code.
cat > "$OUT/sizeof_probe.c" <<'EOF'
#include "ff.h"
unsigned char gp_sizeof_FIL[sizeof(FIL)];
unsigned char gp_sizeof_FATFS[sizeof(FATFS)];
unsigned char gp_FF_MAX_SS[FF_MAX_SS];
EOF

"$CC" "${compile_common[@]}" \
  -c "$OUT/sizeof_probe.c" -o "$OUT/obj/sizeof-stock.obj"
"$CC" "${compile_common[@]}" \
  -DCONFIG_FATFS_USE_DYN_BUFFERS=1 \
  -c "$OUT/sizeof_probe.c" -o "$OUT/obj/sizeof-dynamic.obj"

python3 - "$NM" "$OUT/obj/sizeof-stock.obj" "$OUT/obj/sizeof-dynamic.obj" <<'PY'
import subprocess
import sys
from pathlib import Path

nm = sys.argv[1]
stock_obj = Path(sys.argv[2])
dynamic_obj = Path(sys.argv[3])
required = ("gp_sizeof_FIL", "gp_sizeof_FATFS", "gp_FF_MAX_SS")


def sizes(path: Path) -> dict[str, int]:
    output = subprocess.check_output(
        [nm, "-S", "--defined-only", str(path)], text=True
    )
    found: dict[str, int] = {}
    for line in output.splitlines():
        parts = line.split()
        if len(parts) < 4:
            continue
        name = parts[-1]
        if name in required:
            found[name] = int(parts[1], 16)
    missing = [name for name in required if name not in found]
    if missing:
        raise SystemExit(f"{path}: missing probe symbols {missing}")
    return found


stock = sizes(stock_obj)
dynamic = sizes(dynamic_obj)
if stock["gp_sizeof_FIL"] != 4136 or stock["gp_sizeof_FATFS"] != 4152:
    raise SystemExit(
        "FS1B stock layout drift: "
        f"FIL={stock['gp_sizeof_FIL']} FATFS={stock['gp_sizeof_FATFS']}"
    )
if stock["gp_FF_MAX_SS"] != 4096 or dynamic["gp_FF_MAX_SS"] != 4096:
    raise SystemExit(
        "FS1B sector-size invariant failed: "
        f"stock={stock['gp_FF_MAX_SS']} dynamic={dynamic['gp_FF_MAX_SS']}"
    )
if dynamic["gp_sizeof_FIL"] >= stock["gp_sizeof_FIL"]:
    raise SystemExit("FS1B dynamic FIL layout did not shrink")
if dynamic["gp_sizeof_FATFS"] >= stock["gp_sizeof_FATFS"]:
    raise SystemExit("FS1B dynamic FATFS layout did not shrink")

stock_struct = 5 * stock["gp_sizeof_FIL"] + stock["gp_sizeof_FATFS"]
dynamic_struct = 5 * dynamic["gp_sizeof_FIL"] + dynamic["gp_sizeof_FATFS"]
print(
    "FS1B COMPILER-MEASURED layout: "
    f"FIL {stock['gp_sizeof_FIL']} -> {dynamic['gp_sizeof_FIL']}; "
    f"FATFS {stock['gp_sizeof_FATFS']} -> {dynamic['gp_sizeof_FATFS']}; "
    f"FF_MAX_SS {stock['gp_FF_MAX_SS']} -> {dynamic['gp_FF_MAX_SS']}"
)
print(
    "FS1B COMPILER-MEASURED static-structure class: "
    f"5*FIL+FATFS {stock_struct} -> {dynamic_struct}; "
    f"delta={stock_struct - dynamic_struct} B"
)
PY

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
  "$CC" \
    "${compile_common[@]}" \
    -DCONFIG_FATFS_USE_DYN_BUFFERS=1 \
    -c "$F/$source" -o "$object"
  OBJECTS+=("$object")
done

# This is the compile-time proof that the dynamic path is actually present in
# ff.c, not merely a define written into an unused header.
if ! "$NM" -u "$OUT/obj/ff.c.obj" | grep -Eq '[[:space:]]ff_memalloc$'; then
  echo "FS1B compile proof failed: ff.c.obj does not reference ff_memalloc" >&2
  exit 3
fi
if ! "$NM" -u "$OUT/obj/ff.c.obj" | grep -Eq '[[:space:]]ff_memfree$'; then
  echo "FS1B compile proof failed: ff.c.obj does not reference ff_memfree" >&2
  exit 3
fi

"$AR" rcs "$OUT/libfatfs.a" "${OBJECTS[@]}"
CANDIDATE="$OUT/libfatfs.a"

symbol_names() {
  "$NM" "$@" | awk 'NF >= 2 { print $NF }' | LC_ALL=C sort -u
}

# ABI/rebuild-radius gates. The archive member set and exported symbol names
# remain identical. Object-level undefined references may change only because
# ff.c now uses the allocator functions already owned by FatFs' system port.
diff -u \
  <("$AR" t "$STOCK" | LC_ALL=C sort) \
  <("$AR" t "$CANDIDATE" | LC_ALL=C sort)

diff -u \
  <(symbol_names -g --defined-only "$STOCK") \
  <(symbol_names -g --defined-only "$CANDIDATE")

printf 'FS1B dynamic compile proof: ff.c.obj -> ff_memalloc + ff_memfree\n'
printf 'FS1B source commit: %s\n' "$COMMIT"
printf 'FS1B stock archive: %s\n' "$STOCK"
printf 'FS1B candidate: %s\n' "$CANDIDATE"
sha256sum "$STOCK" "$CANDIDATE"
