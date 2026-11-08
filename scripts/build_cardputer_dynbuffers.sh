#!/usr/bin/env bash
set -euo pipefail

# Build the accepted FS1B Cardputer image without modifying the shared Arduino
# installation. Every stock -lfatfs token in a copied ld_libs response file is
# replaced in-place with the same candidate archive. This preserves the core's
# intentional repeated-library ordering while making a mixed stock/candidate
# link impossible.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
AL="${ARDUINO_IDF_LIBS:-$HOME/.arduino15/packages/m5stack/tools/esp32-arduino-libs/idf-release_v5.4-858a988d-v1/esp32s3}"
STOCK="$AL/lib/libfatfs.a"
STOCK_LD_LIBS="$AL/flags/ld_libs"
export BUILD_PATH="${BUILD_PATH:-${PROJECT_ROOT}/build/cardputer-adv-dynbuffers}"
export ARDUINO_BUILD_PATH="${ARDUINO_BUILD_PATH:-${BUILD_PATH}/.arduino-build}"
FATFS_OUT="${FATFS_OUT:-${BUILD_PATH}/fatfs-dynbuffers}"
export OUT="$FATFS_OUT"

bash "${SCRIPT_DIR}/build_fatfs_dynbuffers_candidate.sh"
CANDIDATE="$FATFS_OUT/libfatfs.a"
R1_LD_LIBS="$FATFS_OUT/ld_libs.r1"

python3 - "$STOCK_LD_LIBS" "$R1_LD_LIBS" "$CANDIDATE" <<'PY'
from pathlib import Path
import re
import sys

src = Path(sys.argv[1])
dst = Path(sys.argv[2])
candidate = str(Path(sys.argv[3]).resolve())
text = src.read_text(encoding="utf-8")

# The pinned core currently repeats -lfatfs in ld_libs. Preserve that exact
# ordering and multiplicity, but replace every occurrence atomically.
stock_tokens = len(re.findall(r"(?<!\S)-lfatfs(?!\S)", text))
if stock_tokens < 1:
    raise SystemExit(f"{src}: no -lfatfs token found")
new_text = re.sub(r"(?<!\S)-lfatfs(?!\S)", candidate, text)
if re.search(r"(?<!\S)-lfatfs(?!\S)", new_text):
    raise SystemExit(f"{dst}: stock -lfatfs token survived replacement")
if new_text.count(candidate) != stock_tokens:
    raise SystemExit(
        f"{dst}: expected {stock_tokens} candidate tokens, "
        f"found {new_text.count(candidate)}"
    )
dst.write_text(new_text, encoding="utf-8")
print(f"FS1B link response: replaced {stock_tokens} -lfatfs token(s) with {candidate}")
PY

bash "${SCRIPT_DIR}/build.sh" \
  --build-property "compiler.c.elf.libs=@${R1_LD_LIBS}" \
  "$@"

mapfile -t MAPS < <(find "$ARDUINO_BUILD_PATH" -type f -name '*.map' -print | sort)
if (( ${#MAPS[@]} != 1 )); then
  printf 'FS1B expected exactly one linker map under %s, found %d\n' \
    "$ARDUINO_BUILD_PATH" "${#MAPS[@]}" >&2
  printf '  %s\n' "${MAPS[@]:-<none>}" >&2
  exit 4
fi
MAP="${MAPS[0]}"
CANDIDATE_ABS="$(realpath "$CANDIDATE")"
STOCK_ABS="$(realpath "$STOCK")"

if ! grep -Fq "$CANDIDATE_ABS" "$MAP"; then
  echo "FS1B link proof failed: candidate archive absent from $MAP" >&2
  exit 5
fi
if grep -Fq "$STOCK_ABS" "$MAP"; then
  echo "FS1B link proof failed: stock and candidate FatFs archives both appear in $MAP" >&2
  exit 6
fi

printf 'FS1B link proof: candidate-only FatFs archive\n'
printf '  map: %s\n' "$MAP"
printf '  candidate: %s\n' "$CANDIDATE_ABS"
printf '  stock absent: %s\n' "$STOCK_ABS"


# Runtime heap recovery does not waive the product static-DRAM gate.
bash "${SCRIPT_DIR}/check_cardputer_dram_budget.sh"   "${BUILD_PATH}/GroovePuter.ino.elf"

echo "=== FS1B dynamic-FatFs Cardputer build PASS ==="
