#!/usr/bin/env bash
# Essentia Discogs-400 models (CC BY-NC-SA 4.0, non-commercial).
# If a URL 404s, take current links from https://essentia.upf.edu/models.html
set -euo pipefail
DIR=${1:-models}
mkdir -p "$DIR"
BASE=https://essentia.upf.edu/models
for f in feature-extractors/discogs-effnet/discogs-effnet-bs64-1.pb \
         classification-heads/genre_discogs400/genre_discogs400-discogs-effnet-1.pb \
         classification-heads/genre_discogs400/genre_discogs400-discogs-effnet-1.json; do
  out="$DIR/$(basename "$f")"
  [[ -s $out ]] && { echo "have $out"; continue; }
  curl -fL --retry 3 -o "$out" "$BASE/$f" || { echo "FAILED $f — check models.html"; exit 1; }
done
echo "OK. Now: python -m gplab selfcheck --config config/pilot_house.yaml --models $DIR"
