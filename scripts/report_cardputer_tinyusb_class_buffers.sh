#!/usr/bin/env bash
set -euo pipefail

BUILD_PATH="${1:?usage: report_cardputer_tinyusb_class_buffers.sh <build-path>}"
PACKAGE_ROOT="${ARDUINO_PACKAGES_ROOT:-${HOME}/.arduino15/packages}"
SYMBOL_PATTERN='ncm_epbuf|_mscd_epbuf|_dfu_epbuf'
CONFIG_PATTERN='CFG_TUD_(NCM|MSC|DFU|DFU_RUNTIME)|CONFIG_TINYUSB_(MSC|DFU|NET|NCM)'

printf '%s\n' "=== TinyUSB class-buffer provenance ==="
printf 'build path: %s\n' "${BUILD_PATH}"
printf 'Arduino package root: %s\n' "${PACKAGE_ROOT}"

map_found=0
while IFS= read -r map_file; do
  map_found=1
  printf '\nLink-map evidence: %s\n' "${map_file}"
  grep -n -E -B 2 -A 2 "${SYMBOL_PATTERN}" "${map_file}" || \
    echo "  candidate symbols not found in this map"
done < <(find "${BUILD_PATH}" -maxdepth 3 -type f -name '*.map' -print 2>/dev/null | sort)
if (( map_found == 0 )); then
  echo "No linker map found below build path."
fi

printf '\nConfiguration evidence:\n'
config_found=0
if [[ -d "${PACKAGE_ROOT}" ]]; then
  while IFS= read -r config_file; do
    matches="$(grep -n -E "${CONFIG_PATTERN}" "${config_file}" 2>/dev/null || true)"
    [[ -n "${matches}" ]] || continue
    config_found=1
    printf '\n%s\n%s\n' "${config_file}" "${matches}"
  done < <(
    find "${PACKAGE_ROOT}" -type f \
      \( -name 'tusb_config.h' -o -name 'sdkconfig.h' -o \
         -name 'sdkconfig' -o -name '*tinyusb*.h' \) \
      -print 2>/dev/null | sort
  )
fi
if (( config_found == 0 )); then
  echo "No matching TinyUSB class macros found in installed package headers."
fi

printf '\nArchive/member hints:\n'
archive_found=0
if [[ -d "${PACKAGE_ROOT}" ]]; then
  while IFS= read -r archive; do
    if command -v nm >/dev/null 2>&1 && \
       nm -A "${archive}" 2>/dev/null | grep -q -E "${SYMBOL_PATTERN}"; then
      archive_found=1
      printf '\n%s\n' "${archive}"
      nm -A "${archive}" 2>/dev/null | grep -E "${SYMBOL_PATTERN}" || true
    fi
  done < <(find "${PACKAGE_ROOT}" -type f -name '*.a' -print 2>/dev/null | sort)
fi
if (( archive_found == 0 )); then
  echo "No candidate definitions found with the host nm; linker-map evidence remains authoritative."
fi

cat <<'EOF'

This report is observational. It does not disable a USB class. A class buffer is
eligible for removal only after its compile-time controlling macro is identified,
a product ELF proves the bytes disappeared, and hardware still enumerates as the
required CDC+MIDI or MIDI-only composite.
EOF

exit 0
