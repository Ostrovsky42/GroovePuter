#!/usr/bin/env python3
"""Independent semantic check for a complete four-track GroovePuter export."""

import pathlib
import struct
import sys

from parse_smf_export_fixture import parse_track


def main(path, expect_file_fixture=False):
    data = pathlib.Path(path).read_bytes()
    if data[:4] != b"MThd" or len(data) < 14:
        raise AssertionError("missing SMF header")
    header_size = int.from_bytes(data[4:8], "big")
    fmt, count, division = struct.unpack(">HHH", data[8:14])
    if header_size != 6 or (fmt, count, division) != (1, 4, 1536):
        raise AssertionError("unexpected format, track count or PPQN")

    position = 8 + header_size
    tracks = []
    for _ in range(count):
        if data[position:position + 4] != b"MTrk":
            raise AssertionError("missing MTrk chunk")
        length = int.from_bytes(data[position + 4:position + 8], "big")
        position += 8
        tracks.append(parse_track(data, position, length))
        position += length
    if position != len(data):
        raise AssertionError("extra data after final track")

    if [track["name"] for track in tracks] != [
            "Conductor", "Synth A", "Synth B", "Drums"]:
        raise AssertionError("unexpected track ordering/names")
    if len(tracks[0]["tempo"]) != 1 or tracks[0]["tempo"][0][0] != 0:
        raise AssertionError("conductor must contain one tempo at tick zero")
    if tracks[0]["time_signature"] != [(0, (4, 2, 24, 8))]:
        raise AssertionError("4/4 time signature missing")
    if any(track["tempo"] or track["time_signature"] for track in tracks[1:]):
        raise AssertionError("tempo/time signature found outside conductor")

    # Keep the deterministic test fixture strict while allowing the parser to
    # validate real project exports at any committed tempo and note content.
    if expect_file_fixture:
        if tracks[0]["tempo"] != [(0, 500000)]:
            raise AssertionError("fixture tempo was not preserved")
        expected = [(4, 60, 0, 163, 100), (4, 60, 6144, 163, 100)]
        if tracks[1]["notes"] != expected:
            raise AssertionError(f"committed pattern mismatch: {tracks[1]['notes']}")
        if tracks[2]["notes"] or tracks[3]["notes"]:
            raise AssertionError("unexpected notes on empty fixture lanes")

    return {"format": fmt, "track_count": count, "division": division,
            "tracks": tracks}


if __name__ == "__main__":
    if len(sys.argv) not in (2, 3) or (len(sys.argv) == 3 and
                                       sys.argv[2] != "--expect-file-fixture"):
        raise SystemExit(
            "usage: parse_smf_export_file.py FILE.mid [--expect-file-fixture]")
    main(sys.argv[1], expect_file_fixture=len(sys.argv) == 3)
