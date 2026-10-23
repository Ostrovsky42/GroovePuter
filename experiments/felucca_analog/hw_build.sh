#!/bin/sh
# SPDX-License-Identifier: MIT
# DSP-B hardware bench build: sh hw_build.sh <fs> <builddir>   (compile only; flash with arduino-cli upload, see REPORT.md)
set -e
FS=$1; B=$2
HERE=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$B"
python3 -I "$HERE/fork22k/tools/gen_engine_tables.py" --fs "$FS" "$HERE/hw_bench/felucca/fe_tables.h"
arduino-cli compile --fqbn "m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app" \
    --build-path "$B" "$HERE/hw_bench" 2>&1 | tail -5
