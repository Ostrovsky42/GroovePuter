#!/usr/bin/env bash
# Genre A/B renderer driver (0.9.24 T01). Host only.
#
# render  build genreab (if needed), render a tag, check every SMF against its
#         TSV and its WAV (note count and bar count), write sha256 into manifest_<tag>.tsv
#           render_ab.sh render --tag base|cand-<task> --out DIR [--takes N] [--genres LIST]
#         defaults: takes 4, genres house,uk_garage,techno,acid,dub,trip_hop
#         the tag is part of every file name: <genre>_t<k>_<tag>.{wav,mid,tsv}
#         MIDI and WAV carry the same 8 bars.
#
# loudness  write <stem>_norm.wav next to every original WAV of a tag, all at one
#         RMS (default -22 dBFS; -20 caps dub_t2 at the peak ceiling). The gain is capped so the peak stays <= -1 dBFS;
#         a capped take is marked in the manifest. Originals are not touched.
#         Gain, RMS and the norm file's sha256 go into manifest_<tag>.tsv.
#           render_ab.sh loudness --out DIR --tag base|cand-<task> [--target -22]
#
# pairs   list A/B pairs for two tags in one directory
#           render_ab.sh pairs --dir DIR --a base --b cand-<task>
#         writes DIR/pairs_<a>_<b>.tsv: genre, take, file A, file B
#
# blind   copy the pairs into BLIND_DIR as blind_<n>_A.wav / _B.wav, taken from the
#         loudness-matched _norm files (both tags must have run `loudness`), with the
#         A/B order shuffled per pair. The key goes to KEYFILE, which must lie outside
#         BLIND_DIR. Open the key only after the listening.
#           render_ab.sh blind --dir DIR --a base --b cand-<task> --blind-dir BDIR --key KEYFILE
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BIN="$ROOT/build/gen_probe/genreab"
CMD="${1:-}"
shift || true

TAG="" OUT="" TAKES=4 GENRES="house,uk_garage,techno,acid,dub,trip_hop"
DIR="" A="" B="" BLIND_DIR="" KEY="" TARGET="-22"
while [ $# -gt 0 ]; do
  case "$1" in
    --tag) TAG="$2"; shift 2 ;;
    --out) OUT="$2"; shift 2 ;;
    --takes) TAKES="$2"; shift 2 ;;
    --genres) GENRES="$2"; shift 2 ;;
    --dir) DIR="$2"; shift 2 ;;
    --a) A="$2"; shift 2 ;;
    --b) B="$2"; shift 2 ;;
    --blind-dir) BLIND_DIR="$2"; shift 2 ;;
    --key) KEY="$2"; shift 2 ;;
    --target) TARGET="$2"; shift 2 ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
done

render() {
  [ -n "$TAG" ] && [ -n "$OUT" ] || { echo "render needs --tag and --out" >&2; exit 2; }
  if [ ! -x "$BIN" ] || [ "$ROOT/tools/gen_probe/genreab.cpp" -nt "$BIN" ]; then
    "$ROOT/tools/gen_probe/build.sh" "$ROOT/tools/gen_probe/genreab.cpp" "$BIN"
  fi
  mkdir -p "$OUT"
  OUT="$(cd "$OUT" && pwd)"
  local log="$OUT/genreab_$TAG.log"
  set +e
  "$BIN" --tag "$TAG" --out "$OUT" --takes "$TAKES" --genres "$GENRES" >"$log" 2>&1
  local rc=$?
  set -e
  grep '^# ' "$log" || true
  [ "$rc" -eq 0 ] || { echo "genreab exited $rc, see $log" >&2; exit "$rc"; }

  # SMF gate: parses, note count equals the TSV rows, bar count equals the WAV.
  local bad=0
  for mid in "$OUT"/*_"$TAG".mid; do
    if ! python3 -I "$ROOT/tools/gen_probe/check_smf.py" "$mid" "${mid%.mid}.tsv" "${mid%.mid}.wav" >/dev/null; then
      echo "SMF CHECK FAILED: $mid" >&2; bad=1
    fi
  done
  [ "$bad" -eq 0 ] || exit 1
  echo "smf: $(ls "$OUT"/*_"$TAG".mid | wc -l) files, note counts and bar counts match their TSV and WAV"

  # Manifest with sha256 of every wav, mid and tsv.
  python3 -I - "$OUT" "$TAG" <<'PY'
import hashlib, os, sys
out, tag = sys.argv[1], sys.argv[2]
src = os.path.join(out, f"manifest_{tag}.tsv")
rows = open(src).read().splitlines()
head = rows[0].split("\t")
lines = [head + ["sha256_wav", "sha256_mid", "sha256_tsv", "tag_version", "wav_path"]]
def sha(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()
for r in rows[1:]:
    cols = r.split("\t")
    stem = os.path.join(out, cols[0][:-4])
    lines.append(cols + [sha(stem + ".wav"), sha(stem + ".mid"), sha(stem + ".tsv"), tag,
                         stem + ".wav"])
with open(src, "w") as f:
    f.write("\n".join("\t".join(l) for l in lines) + "\n")
print(f"manifest: {src} ({len(lines) - 1} takes)")
PY
  echo "sha256 of the take files (tsv/mid/wav) in: $OUT/manifest_$TAG.tsv"
}

loudness() {
  [ -n "$OUT" ] && [ -n "$TAG" ] || { echo "loudness needs --out and --tag" >&2; exit 2; }
  python3 -I - "$OUT" "$TAG" "$TARGET" <<'PY'
import array, hashlib, math, os, sys, wave
out, tag, target_db = sys.argv[1], sys.argv[2], float(sys.argv[3])
CEIL = 10 ** (-1 / 20)  # -1 dBFS peak ceiling, full-scale units
FULL = 32768.0
src = os.path.join(out, f"manifest_{tag}.tsv")
rows = open(src).read().splitlines()
head = rows[0].split("\t")
# Drop norm columns left by an earlier run, so the step can be repeated.
keep = [i for i, h in enumerate(head) if not h.startswith("norm_") and h != "sha256_norm_wav"]
NEW = ["norm_gain_db", "norm_gain", "norm_capped", "norm_rms_dbfs", "norm_peak_dbfs",
       "sha256_norm_wav", "norm_wav_path"]
def read(path):
    with wave.open(path, "rb") as w:
        assert w.getsampwidth() == 2 and w.getnchannels() == 1, path
        rate = w.getframerate()
        s = array.array("h")
        s.frombytes(w.readframes(w.getnframes()))
    return rate, s
def sha(p):
    return hashlib.sha256(open(p, "rb").read()).hexdigest()
lines = ["\t".join([head[i] for i in keep] + NEW)]
report = []
for r in rows[1:]:
    cols = r.split("\t")
    base = [cols[i] for i in keep]
    stem = os.path.join(out, cols[0][:-4])
    rate, s = read(stem + ".wav")
    n = len(s)
    rms = math.sqrt(sum(x * x for x in s) / n) / FULL
    peak = max(abs(x) for x in s) / FULL
    gain = 10 ** ((target_db - 20 * math.log10(rms)) / 20)
    capped = 0
    if peak * gain > CEIL:
        gain = CEIL / peak
        capped = 1
    scaled = array.array("h", (max(-32768, min(32767, round(x * gain))) for x in s))
    norm_path = stem + "_norm.wav"
    with wave.open(norm_path, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(rate)
        w.writeframes(scaled.tobytes())
    nrms = math.sqrt(sum(x * x for x in scaled) / n) / FULL
    npeak = max(abs(x) for x in scaled) / FULL
    lines.append("\t".join(base + [
        f"{20 * math.log10(gain):.2f}", f"{gain:.5f}", str(capped),
        f"{20 * math.log10(nrms):.2f}", f"{20 * math.log10(npeak):.2f}",
        sha(norm_path), norm_path]))
    report.append((20 * math.log10(nrms), 20 * math.log10(npeak), capped))
with open(src, "w") as f:
    f.write("\n".join(lines) + "\n")
print(f"loudness: {len(report)} takes to RMS {target_db:.1f} dBFS, "
      f"capped by the -1 dBFS peak ceiling: {sum(c for _, _, c in report)}")
print("norm RMS %.2f..%.2f dBFS, norm peak %.2f..%.2f dBFS" % (
    min(r[0] for r in report), max(r[0] for r in report),
    min(r[1] for r in report), max(r[1] for r in report)))
PY
}

pairs_list() {
  [ -n "$DIR" ] && [ -n "$A" ] && [ -n "$B" ] || { echo "pairs needs --dir --a --b" >&2; exit 2; }
  local list="$DIR/pairs_${A}_${B}.tsv"
  printf 'genre\ttake\tfile_a\tfile_b\n' > "$list"
  local n=0
  for wa in "$DIR"/*_t*_"$A".wav; do
    [ -e "$wa" ] || continue
    local stem="${wa%_"$A".wav}"
    local wb="${stem}_$B.wav"
    local base; base="$(basename "$stem")"
    if [ ! -e "$wb" ]; then echo "no partner for $(basename "$wa")" >&2; exit 1; fi
    local rest="${base#*_t}"
    printf '%s\t%s\t%s\t%s\n' "${base%_t*}" "${rest%%_*}" "$(basename "$wa")" "$(basename "$wb")" >> "$list"
    n=$((n + 1))
  done
  echo "pairs: $n in $list"
}

blind() {
  [ -n "$DIR" ] && [ -n "$A" ] && [ -n "$B" ] && [ -n "$BLIND_DIR" ] && [ -n "$KEY" ] \
    || { echo "blind needs --dir --a --b --blind-dir --key" >&2; exit 2; }
  case "$KEY" in "$BLIND_DIR"/*) echo "key must lie outside the blind dir" >&2; exit 2 ;; esac
  mkdir -p "$BLIND_DIR" "$(dirname "$KEY")"
  pairs_list >/dev/null
  python3 -I - "$DIR" "$BLIND_DIR" "$KEY" "$DIR/pairs_${A}_${B}.tsv" <<'PY'
import csv, os, random, shutil, sys
d, bdir, key, pairs = sys.argv[1:5]
rng = random.SystemRandom()  # the key file is the only record of the order
rows = list(csv.DictReader(open(pairs), delimiter="\t"))
def norm(name):
    # the pairs name the originals; the blind test plays their loudness-matched copies
    p = os.path.join(d, name[:-4] + "_norm.wav")
    if not os.path.exists(p):
        sys.exit(f"blind: no loudness-matched copy for {name}; run `loudness` for both tags first")
    return p
rng.shuffle(rows)
with open(key, "w") as k:
    k.write("blind_id\tA_is\tB_is\tgenre\ttake\n")
    for i, r in enumerate(rows, 1):
        flip = rng.random() < 0.5
        first, second = (r["file_b"], r["file_a"]) if flip else (r["file_a"], r["file_b"])
        shutil.copyfile(norm(first), os.path.join(bdir, f"blind_{i:02d}_A.wav"))
        shutil.copyfile(norm(second), os.path.join(bdir, f"blind_{i:02d}_B.wav"))
        k.write(f"{i:02d}\t{first}\t{second}\t{r['genre']}\t{r['take']}\n")
print(f"blind: {len(rows)} pairs in {bdir}, key in {key}")
PY
}

case "$CMD" in
  render) render ;;
  loudness) loudness ;;
  pairs) pairs_list ;;
  blind) blind ;;
  *) sed -n '2,30p' "$0"; exit 2 ;;
esac
