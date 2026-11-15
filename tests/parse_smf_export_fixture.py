#!/usr/bin/env python3
"""Small independent semantic parser for the host SMF export fixture."""

import collections
import pathlib
import struct
import sys


def read_vlq(data, offset, limit):
    value = 0
    for _ in range(4):
        if offset >= limit:
            raise AssertionError("truncated VLQ")
        byte = data[offset]
        offset += 1
        value = (value << 7) | (byte & 0x7F)
        if not byte & 0x80:
            return value, offset
    raise AssertionError("overlong VLQ")


def parse_track(data, start, length):
    end = start + length
    if end > len(data):
        raise AssertionError("track exceeds file")
    pos = start
    tick = 0
    running = None
    name = None
    tempo = []
    time_signature = []
    notes = []
    active = collections.defaultdict(collections.deque)
    saw_end = False
    while pos < end:
        delta, pos = read_vlq(data, pos, end)
        tick += delta
        if pos >= end:
            raise AssertionError("missing event status")
        first = data[pos]
        pos += 1
        if first < 0x80:
            if running is None:
                raise AssertionError("running status without channel status")
            status = running
            first_data = first
        else:
            status = first
            first_data = None
            if status < 0xF0:
                running = status

        if status == 0xFF:
            if pos >= end:
                raise AssertionError("truncated meta event")
            meta_type = data[pos]
            pos += 1
            size, pos = read_vlq(data, pos, end)
            payload_end = pos + size
            if payload_end > end:
                raise AssertionError("meta event exceeds track")
            payload = data[pos:payload_end]
            pos = payload_end
            if meta_type == 0x03:
                name = payload.decode("ascii")
            elif meta_type == 0x51:
                if len(payload) != 3:
                    raise AssertionError("invalid tempo payload")
                tempo.append((tick, int.from_bytes(payload, "big")))
            elif meta_type == 0x58:
                if len(payload) != 4:
                    raise AssertionError("invalid time-signature payload")
                time_signature.append((tick, tuple(payload)))
            elif meta_type == 0x2F:
                if payload:
                    raise AssertionError("invalid end-of-track payload")
                saw_end = True
                if pos != end:
                    raise AssertionError("bytes follow end-of-track")
                break
            continue

        if status >= 0xF0:
            raise AssertionError("unsupported system event in fixture")
        kind = status & 0xF0
        channel = status & 0x0F
        if first_data is None:
            if pos >= end:
                raise AssertionError("missing channel data")
            data1 = data[pos]
            pos += 1
        else:
            data1 = first_data
        if kind in (0xC0, 0xD0):
            continue
        if pos >= end:
            raise AssertionError("missing second channel data")
        data2 = data[pos]
        pos += 1
        if kind == 0x90 and data2:
            active[(channel, data1)].append((tick, data2))
        elif kind == 0x80 or (kind == 0x90 and data2 == 0):
            queue = active[(channel, data1)]
            if not queue:
                raise AssertionError("note-off without matching note-on")
            onset, velocity = queue.popleft()
            notes.append((channel, data1, onset, tick - onset, velocity))
    if not saw_end:
        raise AssertionError("track has no end-of-track")
    if any(queue for queue in active.values()):
        raise AssertionError("unclosed note")
    return {"name": name, "tempo": tempo, "time_signature": time_signature,
            "notes": sorted(notes)}


def main(path):
    data = pathlib.Path(path).read_bytes()
    if data[:4] != b"MThd" or len(data) < 14:
        raise AssertionError("missing SMF header")
    header_size = int.from_bytes(data[4:8], "big")
    if header_size != 6:
        raise AssertionError("unexpected header size")
    fmt, count, division = struct.unpack(">HHH", data[8:14])
    if (fmt, count, division) != (1, 2, 1536):
        raise AssertionError(f"unexpected format/count/division: {(fmt, count, division)}")
    pos = 8 + header_size
    tracks = []
    for _ in range(count):
        if data[pos:pos + 4] != b"MTrk":
            raise AssertionError("missing track chunk")
        length = int.from_bytes(data[pos + 4:pos + 8], "big")
        pos += 8
        tracks.append(parse_track(data, pos, length))
        pos += length
    if pos != len(data):
        raise AssertionError("trailing bytes after declared tracks")
    if tracks[0]["name"] != "Conductor":
        raise AssertionError("conductor track name mismatch")
    if tracks[0]["tempo"] != [(0, 500000)]:
        raise AssertionError("tempo was not preserved")
    if tracks[0]["time_signature"] != [(0, (4, 2, 24, 8))]:
        raise AssertionError("time signature was not preserved")
    if tracks[1]["name"] != "Synth A":
        raise AssertionError("synth track name mismatch")
    expected = [(0, 60, 0, 2304, 100), (0, 60, 1536, 1536, 91)]
    if tracks[1]["notes"] != expected:
        raise AssertionError(f"overlapping note round-trip mismatch: {tracks[1]['notes']}")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: parse_smf_export_fixture.py FILE.mid")
    main(sys.argv[1])
