#!/usr/bin/env python3
"""Check a genreab SMF against its TSV (0.9.24 T01 gate).

Usage: check_smf.py <file.mid> <file.tsv> [file.wav]
Parses the SMF independently (header, every MTrk length, running status) and
compares the number of note-on events with the TSV data rows, per channel and
in total. With a WAV, also compares the bar count: the SMF end-of-track tick
(in 4/4 bars of 1920 ticks at 480 PPQ) with the WAV duration times the tempo
from the TSV header. Exit 0 when everything matches, 1 otherwise.
"""
import sys
import wave


def read_vlq(data, pos):
    value = 0
    while True:
        if pos >= len(data):
            raise ValueError("truncated variable-length value")
        b = data[pos]
        pos += 1
        value = (value << 7) | (b & 0x7F)
        if not b & 0x80:
            return value, pos


def parse_smf(path):
    with open(path, "rb") as f:
        data = f.read()
    if data[:4] != b"MThd":
        raise ValueError("missing MThd")
    hlen = int.from_bytes(data[4:8], "big")
    if hlen != 6:
        raise ValueError(f"header length {hlen}, expected 6")
    fmt = int.from_bytes(data[8:10], "big")
    ntrks = int.from_bytes(data[10:12], "big")
    division = int.from_bytes(data[12:14], "big")
    pos = 8 + hlen
    tracks = []
    note_on = {}
    total_on = 0
    for t in range(ntrks):
        if data[pos:pos + 4] != b"MTrk":
            raise ValueError(f"track {t}: missing MTrk at byte {pos}")
        tlen = int.from_bytes(data[pos + 4:pos + 8], "big")
        body = data[pos + 8:pos + 8 + tlen]
        if len(body) != tlen:
            raise ValueError(f"track {t}: declared {tlen} bytes, found {len(body)}")
        pos += 8 + tlen
        p = 0
        status = 0
        ended = False
        ticks = 0
        while p < len(body):
            delta, p = read_vlq(body, p)
            ticks += delta
            b = body[p]
            if b & 0x80:
                status = b
                p += 1
            if status == 0xFF:
                mtype = body[p]
                mlen, p = read_vlq(body, p + 1)
                p += mlen
                if mtype == 0x2F:
                    ended = True
                    break
            elif status in (0xF0, 0xF7):
                slen, p = read_vlq(body, p)
                p += slen
            else:
                kind = status & 0xF0
                ch = status & 0x0F
                n = 1 if kind in (0xC0, 0xD0) else 2
                d1 = body[p]
                d2 = body[p + 1] if n == 2 else 0
                p += n
                if kind == 0x90 and d2 > 0:
                    note_on[ch] = note_on.get(ch, 0) + 1
                    total_on += 1
        if not ended:
            raise ValueError(f"track {t}: no end-of-track meta")
        tracks.append({"length": tlen, "last_tick": ticks})
    if pos != len(data):
        raise ValueError(f"{len(data) - pos} trailing bytes after the last track")
    return {"format": fmt, "tracks": ntrks, "division": division,
            "note_on_total": total_on, "note_on_by_channel": note_on, "track_info": tracks}


def tsv_counts(path):
    total = 0
    by_channel = {}
    with open(path) as f:
        header = None
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            cols = line.rstrip("\n").split("\t")
            if header is None:
                header = cols
                continue
            row = dict(zip(header, cols))
            ch = int(row["channel"]) - 1  # TSV is 1-based, SMF status is 0-based
            by_channel[ch] = by_channel.get(ch, 0) + 1
            total += 1
    return total, by_channel


def tsv_bpm(path):
    with open(path) as f:
        for line in f:
            if line.startswith("#"):
                for field in line.split():
                    if field.startswith("bpm="):
                        return float(field[4:])
    raise ValueError(f"{path}: no bpm= in the TSV header")


def bars_of_wav(path, bpm):
    with wave.open(path, "rb") as w:
        seconds = w.getnframes() / w.getframerate()
    return seconds * bpm / 60.0 / 4.0


def main():
    if len(sys.argv) not in (3, 4):
        print(__doc__)
        return 2
    try:
        smf = parse_smf(sys.argv[1])
    except (ValueError, IndexError) as e:
        print(f"FAIL parse {sys.argv[1]}: {e}")
        return 1
    total, by_channel = tsv_counts(sys.argv[2])
    ok = smf["note_on_total"] == total and smf["note_on_by_channel"] == by_channel
    eot = max(t["last_tick"] for t in smf["track_info"])
    midi_bars = eot / (4 * smf["division"])
    line = (f"format={smf['format']} tracks={smf['tracks']} division={smf['division']} "
            f"note_on={smf['note_on_total']} tsv_rows={total} "
            f"channels_smf={dict(sorted(smf['note_on_by_channel'].items()))} "
            f"channels_tsv={dict(sorted(by_channel.items()))} midi_bars={midi_bars:g}")
    if len(sys.argv) == 4:
        try:
            wav_bars = bars_of_wav(sys.argv[3], tsv_bpm(sys.argv[2]))
        except (ValueError, wave.Error, OSError) as e:
            print(f"FAIL wav {sys.argv[3]}: {e}")
            return 1
        bars_ok = abs(wav_bars - midi_bars) < 0.01 and abs(midi_bars - round(midi_bars)) < 1e-9
        ok = ok and bars_ok
        line += f" wav_bars={wav_bars:.3f} bars_match={'yes' if bars_ok else 'NO'}"
    print(("OK " if ok else "MISMATCH ") + line)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
