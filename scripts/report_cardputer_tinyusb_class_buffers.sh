#!/usr/bin/env bash
set -euo pipefail

BUILD_PATH="${1:?usage: report_cardputer_tinyusb_class_buffers.sh <build-path>}"
PACKAGE_ROOT="${ARDUINO_PACKAGES_ROOT:-${HOME}/.arduino15/packages}"
SYMBOL_PATTERN='ncm_epbuf|_mscd_epbuf|_dfu_epbuf|_transfer_buf'
CONFIG_PATTERN='CFG_TUD_(NCM|MSC|MSC_EP_BUFSIZE|DFU|DFU_RUNTIME|DFU_XFER_BUFSIZE)|CONFIG_TINYUSB_(MSC|DFU|NET|NCM)'

printf '%s\n' "=== TinyUSB class-buffer provenance ==="
printf 'build path: %s\n' "${BUILD_PATH}"
printf 'Arduino package root: %s\n' "${PACKAGE_ROOT}"

map_found=0
symbol_found=0
while IFS= read -r map_file; do
  map_found=1
  printf '\nLink-map evidence: %s\n' "${map_file}"
  matches="$(grep -n -E -B 3 -A 3 "${SYMBOL_PATTERN}" "${map_file}" || true)"
  if [[ -n "${matches}" ]]; then
    symbol_found=1
    printf '%s\n' "${matches}"
  else
    echo "  candidate symbols not found in this map"
  fi
done < <(find "${BUILD_PATH}" -maxdepth 3 -type f -name '*.map' -print 2>/dev/null | sort)
if (( map_found == 0 )); then
  echo "No linker map found below build path."
elif (( symbol_found == 0 )); then
  echo "No candidate class buffers were resolved by the linker maps."
fi

printf '\nConfiguration evidence:\n'
config_found=0
while IFS= read -r config_file; do
  matches="$(grep -n -E "${CONFIG_PATTERN}" "${config_file}" 2>/dev/null || true)"
  [[ -n "${matches}" ]] || continue
  config_found=1
  printf '\n%s\n%s\n' "${config_file}" "${matches}"
done < <(
  {
    if [[ -d "${BUILD_PATH}" ]]; then
      find "${BUILD_PATH}" -maxdepth 5 -type f \
        \( -name 'tusb_config.h' -o -name 'sdkconfig.h' -o \
           -name 'sdkconfig' -o -name '*tinyusb*.h' \) \
        -print 2>/dev/null
    fi
    if [[ -d "${PACKAGE_ROOT}" ]]; then
      find "${PACKAGE_ROOT}" -type f \
        \( -name 'tusb_config.h' -o -name 'sdkconfig.h' -o \
           -name 'sdkconfig' -o -name '*tinyusb*.h' \) \
        -print 2>/dev/null
    fi
  } | sort -u
)
if (( config_found == 0 )); then
  echo "No matching TinyUSB class macros found in generated or installed configuration files."
fi

cat <<'EOF'

This report is observational. It does not disable a USB class. The linker map is
the authoritative source for which archive/object contributed a buffer. A class
is eligible for removal only after its controlling build option is identified,
a product ELF proves the bytes disappeared, and hardware still enumerates as the
required CDC+MIDI or MIDI-only composite.
EOF

exit 0
