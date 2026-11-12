#!/usr/bin/env python3
"""Compare two genrefeatures outputs: what was, what is now (0.9.24 T02 gate).

Usage: features_diff.py OLD NEW
  OLD, NEW   output of tools/gen_probe/genrefeatures (T, G and X rows; '#' lines ignored)

Prints one line per changed value per genre, "genre  metric  was -> now":
  bass.diag   X.bass diagonal (spread of bass onsets inside the genre)
  synthB.diag X.lead diagonal (spread of Synth B onsets inside the genre)
  adjSame     adjacent takes with bar 1 equal (of N-1)
  attA412     bass attacks on steps 4 and 12, mean per bar (G row)
  noRoom      takes where D answered NO ROOM (of N)
  dev+brk8B   takes where D answered DEVELOP + BREAK 8B (of N)
  brk4B       takes where D answered BREAK ONLY 4B (of N; its develop section was skipped)
  grown       takes where D added rows (G row, of N)

Exit codes: 0 no value changed; 1 values changed (table printed);
2 unreadable or malformed file (row width does not match its header, missing
header or X table, genre lists differ). A malformed file is never reported as "no change".
"""
import re
import sys


class FormatError(Exception):
    pass


def is_number(s):
    try:
        float(s)
        return True
    except ValueError:
        return False


def parse(path):
    headers = {}
    rows = {"T": [], "G": []}
    xdata = {}  # "X.bass" -> data rows in genre order
    with open(path, encoding="utf-8") as f:
        for ln, line in enumerate(f, 1):
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            cols = line.split("\t")
            tag = cols[0]
            if tag in ("T", "G") and len(cols) > 1 and cols[1] == "genre":
                headers[tag] = cols
                continue
            if tag in ("T", "G"):
                if tag not in headers:
                    raise FormatError(f"{path}:{ln}: {tag} row before its header")
                if len(cols) != len(headers[tag]):
                    raise FormatError(f"{path}:{ln}: {len(cols)} fields, header has {len(headers[tag])}")
                rows[tag].append(dict(zip(headers[tag], cols)))
                continue
            if tag.startswith("X.") and len(cols) > 2 and is_number(cols[2]):
                xdata.setdefault(tag, []).append(cols)
    if "T" not in headers or "G" not in headers:
        raise FormatError(f"{path}: no T or G header")
    if not rows["T"] or not rows["G"]:
        raise FormatError(f"{path}: no T or G rows")
    for lane in ("X.bass", "X.lead"):
        if lane not in xdata:
            raise FormatError(f"{path}: no {lane} table")
        for r in xdata[lane]:
            if len(r) != len(xdata[lane][0]):
                raise FormatError(f"{path}: {lane} rows differ in width")
    return rows, xdata


def metrics(rows, xdata, path):
    genres = [g["genre"] for g in rows["G"]]
    t_genres = [t["genre"] for t in rows["T"]]
    for g in genres:
        if g not in t_genres:
            raise FormatError(f"{path}: genre {g} has a G row but no T rows")
    for lane in ("X.bass", "X.lead"):
        if len(xdata[lane]) != len(genres):
            raise FormatError(f"{path}: {lane} has {len(xdata[lane])} rows for {len(genres)} genres")
    out = {}
    for i, g in enumerate(genres):
        grow_g = next(r for r in rows["G"] if r["genre"] == g)["grow"]
        takes = [t for t in rows["T"] if t["genre"] == g]
        m = re.search(r"adjSame (\d+)/(\d+)", grow_g)
        if not m:
            raise FormatError(f"{path}: G row {g} has no adjSame field")
        dev = re.search(r"DEVELOP\+BREAK8B (\d+)/(\d+)", grow_g)
        if not dev:
            raise FormatError(f"{path}: G row {g} has no DEVELOP+BREAK8B field")
        out[(g, "bass.diag")] = float(xdata["X.bass"][i][2 + i])
        out[(g, "synthB.diag")] = float(xdata["X.lead"][i][2 + i])
        out[(g, "adjSame")] = f"{m.group(1)}/{m.group(2)}"
        out[(g, "attA412")] = float(next(r for r in rows["G"] if r["genre"] == g)["A.att412"])
        out[(g, "noRoom")] = f"{sum('NO ROOM' in t['grow'] for t in takes)}/{len(takes)}"
        out[(g, "dev+brk8B")] = f"{dev.group(1)}/{dev.group(2)}"
        out[(g, "brk4B")] = f"{sum('BREAK ONLY 4B' in t['grow'] for t in takes)}/{len(takes)}"
        grown = re.search(r"grown (\d+)/(\d+)", grow_g)
        if not grown:
            raise FormatError(f"{path}: G row {g} has no grown field")
        out[(g, "grown")] = f"{grown.group(1)}/{grown.group(2)}"
    return out


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    try:
        old_rows, old_x = parse(sys.argv[1])
        new_rows, new_x = parse(sys.argv[2])
        old = metrics(old_rows, old_x, sys.argv[1])
        new = metrics(new_rows, new_x, sys.argv[2])
    except (FormatError, OSError, StopIteration, IndexError, ValueError) as e:
        print(f"FORMAT ERROR: {e}")
        return 2
    if set(old) != set(new):
        print("FORMAT ERROR: the two files cover different genres or metrics")
        return 2
    changed = [k for k in sorted(old) if old[k] != new[k]]
    if not changed:
        print(f"no change: {len(old)} values compared")
        return 0
    print("genre\tmetric\twas\tnow")
    for g, metric in changed:
        was, now = old[(g, metric)], new[(g, metric)]
        if isinstance(was, float):
            print(f"{g}\t{metric}\t{was:.2f}\t{now:.2f}")
        else:
            print(f"{g}\t{metric}\t{was}\t{now}")
    print(f"changed: {len(changed)} of {len(old)} values")
    return 1


if __name__ == "__main__":
    sys.exit(main())
