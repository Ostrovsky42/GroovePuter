import importlib.util
import pathlib
import struct
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
PARSER_PATH = ROOT / "tests/parse_smf_export_file.py"
spec = importlib.util.spec_from_file_location("smf_export_file_parser", PARSER_PATH)
parser = importlib.util.module_from_spec(spec)
spec.loader.exec_module(parser)


def vlq(value):
    encoded = [value & 0x7F]
    value >>= 7
    while value:
        encoded.insert(0, (value & 0x7F) | 0x80)
        value >>= 7
    return bytes(encoded)


def track(name, events=b""):
    body = b"\x00\xff\x03" + vlq(len(name)) + name.encode("ascii")
    body += events + b"\x00\xff\x2f\x00"
    return b"MTrk" + struct.pack(">I", len(body)) + body


tempo = 857143  # The committed project tempo in the SDL export, 70 BPM.
conductor = b"\x00\xff\x51\x03" + tempo.to_bytes(3, "big")
conductor += b"\x00\xff\x58\x04\x04\x02\x18\x08"
note_events = b"\x00\x94\x3c\x64" + vlq(1536) + b"\x84\x3c\x00"
tracks = [
    track("Conductor", conductor),
    track("Synth A", note_events),
    track("Synth B"),
    track("Drums"),
]
data = b"MThd" + struct.pack(">IHHH", 6, 1, 4, 1536) + b"".join(tracks)

with tempfile.TemporaryDirectory(prefix="gp-smf-parser-") as directory:
    path = pathlib.Path(directory) / "seventy-bpm.mid"
    path.write_bytes(data)
    parsed = parser.main(path)

assert parsed["tracks"][0]["tempo"] == [(0, tempo)]
assert parsed["tracks"][1]["notes"] == [(4, 60, 0, 1536, 100)]
