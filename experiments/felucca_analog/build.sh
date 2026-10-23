#!/bin/sh
# SPDX-License-Identifier: MIT
# DSP-B host build: sh build.sh <fs> <builddir> [san]
#   san: build with ASan/UBSan
# The engine sources are fork22k/ (vendor/felucca/ plus the changes in FORK.md); the tables come from the fork's generator.
set -e
FS=$1; B=$2; SAN=${3:-}
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
FORK="$HERE/fork22k"
mkdir -p "$B"
python3 -I "$FORK/tools/gen_engine_tables.py" --fs "$FS" "$B/fe_tables.h"
FLAGS="-O2 -g -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -Wno-sign-compare"
if [ "$SAN" = san ]; then FLAGS="$FLAGS -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer"; fi
cc $FLAGS -std=c99 -I"$FORK" -I"$B" -c "$HERE/fa_analog.c" -o "$B/fa_analog.o"
c++ $FLAGS -std=c++17 -I"$ROOT/src/dsp" -I"$HERE" -I"$B" -c "$HERE/fa_driver.cpp" -o "$B/fa_driver.o"
c++ $FLAGS -std=c++17 -I"$ROOT/src/dsp" -c "$ROOT/src/dsp/sh101_synth_voice.cpp" -o "$B/sh101.o"
c++ $FLAGS -std=c++17 -I"$ROOT/src/dsp" -c "$ROOT/src/dsp/wave_morph_synth_voice.cpp" -o "$B/wavemorph.o"
c++ $FLAGS -o "$B/fa_driver" "$B/fa_driver.o" "$B/fa_analog.o" "$B/sh101.o" "$B/wavemorph.o" -lm
echo "built $B/fa_driver (fs=$FS${SAN:+, $SAN})"
