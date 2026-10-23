#!/bin/sh
# SPDX-License-Identifier: MIT
# DSP-B blind package: renders the three materials (bass / lead / high) for the native 22.05 kHz Felucca, the 44.1 kHz
# Felucca with FIR decimation, SH101 and WAVEMORPH, then packs them. Output: out/blind-pack/ (no key) and out/blind-key/.
#   sh blind/run_blind.sh [seed]
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
EXP=$(cd "$HERE/.." && pwd)
SEED=${1:-20261024}
OUT="$EXP/out"
BUILD="$OUT/build"
STIM="$OUT/blind-stim"
PY="python3 -I $EXP/analyze.py"
mkdir -p "$STIM"
sh "$EXP/build.sh" 22050 "$BUILD/22050" >/dev/null
sh "$EXP/build.sh" 44100 "$BUILD/44100" >/dev/null

# material: name preset transpose
for spec in "bass 0 0" "lead 3 12" "high 0 36"; do
    set -- $spec
    mat=$1; preset=$2; tr=$3
    t=""; [ "$tr" != 0 ] && t="_t$tr"
    mkdir -p "$OUT/blind-r-$mat"
    "$BUILD/22050/fa_driver" 22050 "$OUT/blind-r-$mat" render preset=$preset transpose=$tr > /dev/null
    "$BUILD/44100/fa_driver" 44100 "$OUT/blind-r-$mat" render preset=$preset transpose=$tr > /dev/null
    R="$OUT/blind-r-$mat"
    $PY decimate "$R/melody_FELUCCA_ANALOG_44100_p${preset}${t}.wav" "$R/melody_FELUCCA_DEC_22050_p${preset}${t}.wav" > /dev/null
    cp "$R/melody_FELUCCA_ANALOG_22050_p${preset}${t}.wav" "$STIM/${mat}_FELUCCA_NATIVE_22050.wav"
    cp "$R/melody_FELUCCA_DEC_22050_p${preset}${t}.wav" "$STIM/${mat}_FELUCCA_RESAMPLED_44100.wav"
    cp "$R/melody_SH101_22050_p${preset}${t}.wav" "$STIM/${mat}_SH101.wav"
    cp "$R/melody_WAVEMORPH_22050_p${preset}${t}.wav" "$STIM/${mat}_WAVEMORPH.wav"
done
python3 -I "$HERE/make_blind.py" "$STIM" "$OUT/blind-pack" "$OUT/blind-key" "$SEED"
