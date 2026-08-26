#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SDL_DIR="${ROOT_DIR}/platform_sdl"
BUILD_DIR="${ROOT_DIR}/build/m2-a1"
mkdir -p "${BUILD_DIR}"

PY_TEST="${ROOT_DIR}/tests/test_m2_crossbar_note_lifetime_contract.py"
RUNTIME_BIN="${SDL_DIR}/m2_a1_crossbar_note_lifetime"

python3 "${PY_TEST}" > "${BUILD_DIR}/source-run1.txt"
python3 "${PY_TEST}" > "${BUILD_DIR}/source-run2.txt"
diff -u "${BUILD_DIR}/source-run1.txt" "${BUILD_DIR}/source-run2.txt"
cat "${BUILD_DIR}/source-run1.txt"

DEFAULT_SOURCES="$(make -C "${SDL_DIR}" -pn 2>/dev/null | sed -n 's/^SOURCES := //p' | head -n 1)"
if [[ -z "${DEFAULT_SOURCES}" ]]; then
  echo "M2-A1: failed to read platform_sdl SOURCES" >&2
  exit 1
fi

# Link the real SDL MiniAcid implementation, but replace sdl_main.cpp with the
# research-only runtime characterization and add the normal host USB MIDI sink.
RUNTIME_SOURCES=" ${DEFAULT_SOURCES} "
RUNTIME_SOURCES="${RUNTIME_SOURCES// sdl_main.cpp / }"
RUNTIME_SOURCES="${RUNTIME_SOURCES} ../tests/test_m2_crossbar_note_lifetime_runtime.cpp ../src/midi/usb_midi_output.cpp"

# bits/stdc++.h is host-test-only here. Preloading the standard library before
# the test-local `#define private public` prevents that access shim from touching
# libstdc++/Clang standard-library class declarations.
BASE_FLAGS="-std=c++17 -I.. -I. -include bits/stdc++.h -include arduino_compat.h -DUSE_RETRO_THEME -DUSE_AMBER_THEME"

run_runtime() {
  local cxx="$1"
  local label="$2"
  local extra_flags="${3:-}"
  local output="${BUILD_DIR}/runtime-${label}.txt"

  rm -f "${RUNTIME_BIN}"
  make -C "${SDL_DIR}" \
    CXX="${cxx}" \
    TARGET="m2_a1_crossbar_note_lifetime" \
    SOURCES="${RUNTIME_SOURCES}" \
    CXXFLAGS="${BASE_FLAGS} ${extra_flags}" \
    all

  if [[ "${label}" == "asan-ubsan" ]]; then
    ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
      "${RUNTIME_BIN}" > "${output}" 2>&1
  else
    "${RUNTIME_BIN}" > "${output}" 2>&1
  fi

  grep -E '^(A|B|C|D|E|F|G|H) PASS:|^M2-A1 runtime characterization:' \
    "${output}" > "${BUILD_DIR}/runtime-${label}-canonical.txt"
  cat "${BUILD_DIR}/runtime-${label}-canonical.txt"
}

run_runtime g++ gcc
run_runtime clang++ clang

diff -u \
  "${BUILD_DIR}/runtime-gcc-canonical.txt" \
  "${BUILD_DIR}/runtime-clang-canonical.txt"

run_runtime g++ asan-ubsan "-fsanitize=address,undefined -fno-omit-frame-pointer"
diff -u \
  "${BUILD_DIR}/runtime-gcc-canonical.txt" \
  "${BUILD_DIR}/runtime-asan-ubsan-canonical.txt"

if grep -E 'AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:' \
     "${BUILD_DIR}/runtime-asan-ubsan.txt" >/dev/null; then
  echo "M2-A1: sanitizer diagnostic detected" >&2
  cat "${BUILD_DIR}/runtime-asan-ubsan.txt" >&2
  exit 1
fi

echo "M2-A1 execution closure: PASS"
