#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RECIPE = (ROOT / "scripts/build_fatfs_dynbuffers_candidate.sh").read_text(encoding="utf-8")
BUILD = (ROOT / "scripts/build_cardputer_dynbuffers.sh").read_text(encoding="utf-8")
UPLOAD = (ROOT / "scripts/upload_cardputer_dynbuffers.sh").read_text(encoding="utf-8")

assert "COMMIT=858a988d" in RECIPE
assert "CONFIG_FATFS_USE_DYN_BUFFERS 1" in RECIPE
assert "libfatfs.a" in RECIPE
assert "member set: identical" in RECIPE
assert "defined globals: identical" in RECIPE
assert "undefined symbols: identical" in RECIPE

# The shared Arduino/M5Stack SDK must remain read-only. The candidate lives in
# FATFS_OUT and only the disposable overlay is removed/rebuilt.
assert 'OUT="${OUT:-/tmp/fatfs-build}"' in RECIPE
assert 'rm -rf "$OUT"' in RECIPE
assert 'rm -rf "${SDK_OVERLAY}"' in BUILD
assert 'rm -rf "${SDK_SOURCE}"' not in BUILD
assert 'rm -f "${SDK_SOURCE}/lib/libfatfs.a"' not in BUILD
assert 'compiler.sdk.path=${SDK_OVERLAY}' in BUILD
assert 'ln -s "${FATFS_OUT}/libfatfs.a" "${SDK_OVERLAY}/lib/libfatfs.a"' in BUILD

# A successful combined build must prove one coherent FatFs archive in the map.
assert 'CANDIDATE_PATH="${SDK_OVERLAY}/lib/libfatfs.a"' in BUILD
assert 'STOCK_PATH="${SDK_SOURCE}/lib/libfatfs.a"' in BUILD
assert 'grep -Fq "${CANDIDATE_PATH}" "${MAP}"' in BUILD
assert 'grep -Fq "${STOCK_PATH}" "${MAP}"' in BUILD
assert "candidate only" in BUILD

# Upload always builds the dynamic-buffer profile first and preserves the
# existing static DRAM budget gate before flashing.
assert 'build_cardputer_dynbuffers.sh' in UPLOAD
assert 'check_cardputer_dram_budget.sh' in UPLOAD
assert '--input-dir "${BUILD_PATH}"' in UPLOAD

# The product profile must not silently enable diagnostic handle-census code.
for text in (RECIPE, BUILD, UPLOAD):
    assert "GROOVEPUTER_SD_HANDLE_CENSUS" not in text

print("PASS: UI+FS1 build profile is isolated, reproducible and map-proven")
