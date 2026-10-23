#!/bin/sh
# SPDX-License-Identifier: MIT
# DSP-B reproduction. Builds the host driver for the native 22.05 kHz fork and the 44.1 kHz reference, renders the WAVs,
# runs the analysis, the 44.1 -> 22.05 decimation test and the sanitizers. Everything goes to out/ (git-ignored):
# the WAVs, the tables in *.md, checksums.sha256. The decimation is slow (pure Python FIR): expect a few minutes.
#   sh run_all.sh
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
OUT="$HERE/out"
BUILD="$OUT/build"
PY="python3 -I $HERE/analyze.py"
rm -rf "$OUT"
mkdir -p "$OUT" "$BUILD"
for fs in 22050 44100; do sh "$HERE/build.sh" "$fs" "$BUILD/$fs" >/dev/null; done
D22="$BUILD/22050/fa_driver"
D44="$BUILD/44100/fa_driver"

# melodies: every preset at the native rate, INIT at 44.1 (the reference) and its decimated copy
mkdir -p "$OUT/mel"
for p in 0 1 2 3; do "$D22" 22050 "$OUT/mel" render preset=$p > "$OUT/mel/render22-p$p.txt"; done
"$D44" 44100 "$OUT/mel" render preset=0 > "$OUT/mel/render44-p0.txt"
$PY decimate "$OUT/mel/melody_FELUCCA_ANALOG_44100_p0.wav" "$OUT/mel/melody_FELUCCA_ANALOG_22050_p0_dec.wav"
$PY melody "$OUT/mel" 22050 > "$OUT/melody-22050.md"
# (the 44.1 reference melody is the sweep and level data)

# levels: held notes per preset at both rates, raw gain (65536) so the gain can be chosen from the numbers
for fs in 22050 44100; do
    D=$([ $fs = 22050 ] && echo "$D22" || echo "$D44")
    mkdir -p "$OUT/lvl-$fs"
    for p in 0 1 2 3; do for m in 36 48 60 72 84; do
        "$D" $fs "$OUT/lvl-$fs" sustain $m 1.0 preset=$p gain=65536 > /dev/null
    done; done
    $PY levels "$OUT/lvl-$fs" $fs > "$OUT/levels-$fs.md"
done

# sweeps: one held note per MIDI number. normal filter 24..96 (musical range), filter open 24..122 (the alias test)
for fs in 22050 44100; do
    D=$([ $fs = 22050 ] && echo "$D22" || echo "$D44")
    mkdir -p "$OUT/sw-$fs"
    "$D" $fs "$OUT/sw-$fs" sweep 24 96 > /dev/null
    "$D" $fs "$OUT/sw-$fs" sweep 24 122 bright > /dev/null
done
# the 44.1 kHz sweeps, decimated 2:1 with the FIR, become the third column of the comparison
mkdir -p "$OUT/dec"
for f in "$OUT"/sw-44100/sweep_44100_m*.wav; do
    b=$(basename "$f" .wav)
    m=$(echo "$b" | sed 's/.*_m\([0-9]*\).*/\1/')
    case "$b" in *_bright*) sfx=_bright ;; *) sfx= ;; esac
    $PY decimate "$f" "$OUT/dec/sweep_22050_m${m}${sfx}_p0_dec.wav" > /dev/null
done
cp "$OUT"/dec/*.wav "$OUT/sw-22050/" 2>/dev/null || true
$PY sweep "$OUT/sw-22050" 22050 24 96 > "$OUT/sweep-22050-native.md"
$PY sweep "$OUT/sw-22050" 22050 24 122 bright > "$OUT/sweep-22050-native-bright.md"
$PY sweep "$OUT/sw-22050" 22050 24 96 dec > "$OUT/sweep-22050-dec.md"
$PY sweep "$OUT/sw-22050" 22050 24 122 bright dec > "$OUT/sweep-22050-dec-bright.md"
$PY sweep "$OUT/sw-44100" 44100 24 96 > "$OUT/sweep-44100-ref.md"
$PY sweep "$OUT/sw-44100" 44100 24 122 bright > "$OUT/sweep-44100-ref-bright.md"

# the FIR itself (stopband, ripple)
$PY decimate "$OUT/mel/melody_FELUCCA_ANALOG_44100_p0.wav" "$OUT/mel/_fir_check.wav" > "$OUT/fir.txt"
rm -f "$OUT/mel/_fir_check.wav"

# sanitizers and stress, at each rate
for fs in 22050 44100; do
    sh "$HERE/build.sh" "$fs" "$BUILD/$fs-san" san >/dev/null
    "$BUILD/$fs-san/fa_driver" "$fs" "$BUILD/$fs-san" stress 5000 > "$OUT/stress-san-$fs.txt" 2> "$OUT/stress-san-$fs.err"
    test ! -s "$OUT/stress-san-$fs.err"
done
(cd "$OUT" && find . -path ./build -prune -o -name '*.wav' -print | sort | xargs sha256sum > checksums.sha256)
echo "done: $OUT"
