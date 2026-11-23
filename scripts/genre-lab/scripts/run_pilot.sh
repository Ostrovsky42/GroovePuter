#!/usr/bin/env bash
# Full pilot run. Each step is resumable; stop after any step and inspect.
set -euo pipefail

CONFIG=${CONFIG:-config/pilot_house.yaml}
JAMENDO=${JAMENDO:-data/mtg-jamendo-dataset}          # git clone of MTG/mtg-jamendo-dataset
AUDIO=${AUDIO:-data/jamendo-audio}                     # unpacked audio root (PATH column)
MODELS=${MODELS:-models}
OUT=${OUT:-out/$(basename "$CONFIG" .yaml)}
DRUMS=${DRUMS:-bands}                                  # bands | midi (ADTOF -> <stem>.mid)
PY="python -m gplab"
export PYTHONPATH=src

mkdir -p "$OUT"
echo "== 0. selfcheck"
$PY selfcheck --config "$CONFIG" --models "$MODELS"

echo "== 1. tags (check jamendo_tags in config against this list)"
$PY tags --tsv "$JAMENDO/data/autotagging_genre.tsv" | head -60

echo "== 2. candidates"
$PY candidates --tsv "$JAMENDO/data/autotagging_genre.tsv" --config "$CONFIG" --out "$OUT/candidates.csv"

echo "== 3. styles (slow, resumable)"
$PY styles --candidates "$OUT/candidates.csv" --audio-root "$AUDIO" --models "$MODELS" \
  --config "$CONFIG" --out "$OUT/styles.jsonl"

echo "== 4. select -> manifest + GO/NO-GO"
$PY select --candidates "$OUT/candidates.csv" --styles "$OUT/styles.jsonl" --config "$CONFIG" --out "$OUT"
read -r -p "Review $OUT/selection_report.md. Continue? [y/N] " ok; [[ $ok == y ]] || exit 0

echo "== 5. analyze (slowest, resumable)"
$PY analyze --manifest "$OUT/manifest.csv" --audio-root "$AUDIO" --config "$CONFIG" \
  --out "$OUT/bars" --drums "$DRUMS" --bass basicpitch

echo "== 6. calibrate thresholds on real music"
$PY holdout --bars "$OUT/bars" --config "$CONFIG" --out "$OUT/calibration" --seeds 10

echo "== 7. profiles + header"
$PY profile --bars "$OUT/bars" --config "$CONFIG" --out "$OUT/profiles.json" --corpus-label "MTG-Jamendo $(date +%F)"
$PY codegen --profiles "$OUT/profiles.json" --out "$OUT/genre_lab_profiles.h"
echo "Done. Next: put calibration.md thresholds into config (separate commit), then PR-4."
