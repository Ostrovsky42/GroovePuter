#!/usr/bin/env python3
import argparse
import os
import subprocess
import tempfile
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

CASES = (
    ("ADDED", "DrumAndBass", "DnB", 5, "B", "Melodic", "MINOR FALL", "0000", "8080", True),
    ("ADDED", "TripHop", "TripHop", 4, "A", "Bass", "II-V-I", "0000", "8080", True),
    ("ADDED", "House", "House", 4, "A", "Bass", "POP CYCLE", "0000", "8080", True),
    ("REDUCED", "House", "House", 5, "B", "Melodic", "POP CYCLE", "4904", "8080", True),
    ("REDUCED", "Outrun", "Outrun", 0, "B", "Melodic", "POP CYCLE", "2448", "8080", True),
    ("RELOCATED", "UkGarage", "UKG", 1, "B", "Melodic", "BORROWED LIFT", "0101", "8080", True),
    ("RELOCATED", "FunkSoul", "FunkSoul", 6, "B", "Chord+Mel fill", "BORROWED LIFT", "0802", "8080", True),
    ("CHORD", "TripHop", "TripHop", 2, "B", "Chord", "PARALLEL SHIFT", "0902", "8080", True),
    ("STATIC CTRL", "Acid", "Acid", 2, "B", "Control", "STATIC MODAL", None, None, False),
    ("STATIC CTRL", "Techno", "Techno", 4, "B", "Control", "PEDAL DRONE", None, None, False),
    ("MOVING CTRL", "Reggae", "Reggae", 4, "B", "Control", "BORROWED LIFT", "0202", "8080", False),
)

HARMONIC_BLOCK = """  HarmonicRhythmRequest harmonicRequest{};
  harmonicRequest.progression = result.progressionId;
  const HarmonicRhythmResult harmonic =
      realizeHarmonicRhythm(harmonicRequest);
  result.harmonicRhythmStatus = harmonic.status;
  result.harmonicEventOnsets = harmonic.plan.onsets;
  result.harmonicEventCount = harmonic.plan.eventCount;
  if (harmonic.status != HarmonicRhythmStatus::Ok) {
    result.status = StrongRhythmMigrationStatus::InvalidContext;
    return result;
  }

"""
MISMATCH_BLOCK = """  if (progression.plan.eventCount != harmonic.plan.eventCount) {
    result.status = StrongRhythmMigrationStatus::InvalidContext;
    return result;
  }

"""
NEW_COUNT = "  progressionRequest.harmonicEventCount = harmonic.plan.eventCount;\n"
OLD_COUNT = "  progressionRequest.harmonicEventCount = onsetCount(chord.plan.onsets);\n"

SOURCES = (
    "src/generation/generation_context.cpp",
    "src/generation/composition/rhythm_selection.cpp",
    "src/generation/composition/generation_profile.cpp",
    "src/generation/composition/tonal_profile.cpp",
    "src/generation/feel/feel_interpreter.cpp",
    "src/generation/feel/feel_pattern_adapter.cpp",
    "src/generation/rhythm/rhythm_catalog.cpp",
    "src/generation/rhythm/reference_vocabulary.cpp",
    "src/generation/rhythm/relationship_resolver.cpp",
    "src/generation/rhythm/rhythm_realizer.cpp",
    "src/generation/materialization/pattern_materializer.cpp",
    "src/generation/roles/semantic_pattern_projector.cpp",
    "src/generation/roles/bass_rhythm.cpp",
    "src/generation/roles/bass_pitch_behavior.cpp",
    "src/generation/roles/chord_rhythm.cpp",
    "src/generation/roles/chord_progression.cpp",
    "src/generation/roles/melodic_motif.cpp",
    "src/generation/roles/melodic_pitch_intent.cpp",
    "src/generation/tonal/tonal_projector.cpp",
    "src/generation/tonal/tonal_materializer.cpp",
    "src/generation/migration/tonal_pattern_adapter.cpp",
)


@dataclass(frozen=True)
class Row:
    index: int
    mode: str
    ordinal: int
    voice: str
    progression: str
    clock: str
    bpm: int
    drums: bytes
    synth_a: bytes
    synth_b: bytes


def reverse_f08_source(source: str) -> str:
    if source.count(HARMONIC_BLOCK) != 1:
        raise AssertionError("F08 harmonic owner block moved; review reverse patch")
    source = source.replace(HARMONIC_BLOCK, "", 1)

    if source.count(NEW_COUNT) != 1:
        raise AssertionError("F08 progression count handoff moved; review reverse patch")
    source = source.replace(NEW_COUNT, OLD_COUNT, 1)

    if source.count(MISMATCH_BLOCK) != 1:
        raise AssertionError("F08 progression/harmonic count guard moved; review reverse patch")
    source = source.replace(MISMATCH_BLOCK, "", 1)

    count = source.count("harmonic.plan.onsets")
    if count != 5:
        raise AssertionError(
            f"expected five F08 tonal harmonic-clock consumers after owner removal, got {count}"
        )
    source = source.replace("harmonic.plan.onsets", "chord.plan.onsets")

    if "HarmonicRhythmRequest harmonicRequest" in source:
        raise AssertionError("temporary legacy source still contains HarmonicRhythm owner")
    if NEW_COUNT in source:
        raise AssertionError("temporary legacy source still uses HarmonicRhythm event count")
    if source.count("chord.plan.onsets") < 6:
        raise AssertionError("temporary legacy source did not restore chord-derived timing")
    return source


def compile_dump(migration_source: Path, output: Path) -> None:
    cxx = os.environ.get("CXX", "g++")
    cmd = [
        cxx,
        "-std=c++17",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-Wvla",
        "-Wno-c++20-extensions",
        "-Wno-unused-but-set-variable",
        f"-I{ROOT}",
        f"-I{ROOT / 'src/generation/migration'}",
    ]
    cmd.extend(str(ROOT / source) for source in SOURCES)
    cmd.extend(
        (
            str(migration_source),
            str(ROOT / "tests/dump_0_9_9_f08_listen_fixture.cpp"),
            "-o",
            str(output),
        )
    )
    subprocess.run(cmd, check=True)


def run_dump(executable: Path, mode: str) -> list[Row]:
    completed = subprocess.run(
        [str(executable), mode],
        check=True,
        text=True,
        stdout=subprocess.PIPE,
    )
    lines = completed.stdout.splitlines()
    expected_header = (
        "index\tmode\tordinal\tvoice\tprogression\tclock\tbpm\t"
        "drums\tsynth_a\tsynth_b"
    )
    if not lines or lines[0] != expected_header:
        raise AssertionError(f"unexpected listen dump header: {lines[:1]!r}")

    rows: list[Row] = []
    for line_number, line in enumerate(lines[1:], start=2):
        fields = line.split("\t")
        if len(fields) != 10:
            raise AssertionError(f"dump line {line_number}: expected 10 fields, got {len(fields)}")
        index, mode_name, ordinal, voice, progression, clock, bpm, drums, synth_a, synth_b = fields
        rows.append(
            Row(
                index=int(index),
                mode=mode_name,
                ordinal=int(ordinal),
                voice=voice,
                progression=progression,
                clock=clock,
                bpm=int(bpm),
                drums=bytes.fromhex(drums),
                synth_a=bytes.fromhex(synth_a),
                synth_b=bytes.fromhex(synth_b),
            )
        )
    return rows


def synth_non_note_bytes(data: bytes) -> bytes:
    if len(data) % 9 != 0:
        raise AssertionError(f"invalid synth fixture byte count: {len(data)}")
    return b"".join(data[offset + 1 : offset + 9] for offset in range(0, len(data), 9))


def validate(old_rows: list[Row], new_rows: list[Row]) -> None:
    if len(old_rows) != len(CASES) or len(new_rows) != len(CASES):
        raise AssertionError(
            f"listen corpus size moved: old={len(old_rows)} new={len(new_rows)} expected={len(CASES)}"
        )

    for zero_based, (old, new, case) in enumerate(zip(old_rows, new_rows, CASES)):
        group, mode, _short_mode, ordinal, voice, _focus, progression, old_clock, new_clock, changed = case
        expected_index = zero_based + 1
        expected_identity = (expected_index, mode, ordinal, voice, progression)
        old_identity = (old.index, old.mode, old.ordinal, old.voice, old.progression)
        new_identity = (new.index, new.mode, new.ordinal, new.voice, new.progression)
        if old_identity != expected_identity or new_identity != expected_identity:
            raise AssertionError(
                f"case {expected_index} identity moved: old={old_identity} new={new_identity} "
                f"expected={expected_identity}"
            )

        if old_clock is not None and old.clock != old_clock:
            raise AssertionError(f"case {expected_index} old clock {old.clock} != {old_clock}")
        if new_clock is not None and new.clock != new_clock:
            raise AssertionError(f"case {expected_index} new clock {new.clock} != {new_clock}")
        if old.bpm <= 0 or old.bpm != new.bpm:
            raise AssertionError(
                f"case {expected_index} BPM mismatch/invalid: old={old.bpm} new={new.bpm}"
            )
        if old.drums != new.drums:
            raise AssertionError(f"case {expected_index} F08 changed drum material")
        if synth_non_note_bytes(old.synth_a) != synth_non_note_bytes(new.synth_a):
            raise AssertionError(f"case {expected_index} F08 changed Synth A non-pitch fields")
        if synth_non_note_bytes(old.synth_b) != synth_non_note_bytes(new.synth_b):
            raise AssertionError(f"case {expected_index} F08 changed Synth B non-pitch fields")

        selected_old = old.synth_a if voice == "A" else old.synth_b
        selected_new = new.synth_a if voice == "A" else new.synth_b
        selected_changed = selected_old != selected_new
        if selected_changed != changed:
            raise AssertionError(
                f"case {expected_index} selected-voice change moved: "
                f"changed={selected_changed}, expected={changed} ({group})"
            )


def cpp_string(value: str) -> str:
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def emit_byte_matrix(name: str, rows: list[bytes], width_name: str) -> list[str]:
    output = [f"inline constexpr uint8_t {name}[kCaseCount][{width_name}] = {{"]
    for row in rows:
        output.append("  {")
        for offset in range(0, len(row), 16):
            chunk = ", ".join(f"0x{value:02x}" for value in row[offset : offset + 16])
            output.append(f"    {chunk},")
        output.append("  },")
    output.append("};")
    output.append("")
    return output


def generate_header(old_rows: list[Row], new_rows: list[Row]) -> str:
    drum_bytes = len(old_rows[0].drums)
    synth_bytes = len(old_rows[0].synth_a)
    if any(len(row.drums) != drum_bytes for row in old_rows + new_rows):
        raise AssertionError("drum fixture byte width is not fixed")
    if any(
        len(data) != synth_bytes
        for row in old_rows + new_rows
        for data in (row.synth_a, row.synth_b)
    ):
        raise AssertionError("synth fixture byte width is not fixed")

    lines = [
        "#pragma once",
        "",
        "#include <cstdint>",
        "",
        "namespace GroovePuterRhythm {",
        "namespace F08ListenFixtureData {",
        "",
        "struct CaseMeta {",
        "  const char* group;",
        "  const char* mode;",
        "  uint8_t ordinal;",
        "  char voice;",
        "  const char* focus;",
        "  const char* progression;",
        "  const char* oldClock;",
        "  const char* newClock;",
        "  uint16_t bpm;",
        "  bool fingerprintChanged;",
        "};",
        "",
        f"inline constexpr uint8_t kCaseCount = {len(CASES)};",
        f"inline constexpr uint16_t kDrumBytes = {drum_bytes};",
        f"inline constexpr uint16_t kSynthBytes = {synth_bytes};",
        "",
        "inline constexpr CaseMeta kCases[kCaseCount] = {",
    ]
    for old, new, case in zip(old_rows, new_rows, CASES):
        group, _mode, short_mode, ordinal, voice, focus, progression, _old_clock, _new_clock, changed = case
        lines.append(
            "  {"
            + ", ".join(
                (
                    cpp_string(group),
                    cpp_string(short_mode),
                    str(ordinal),
                    f"'{voice}'",
                    cpp_string(focus),
                    cpp_string(progression),
                    cpp_string(old.clock),
                    cpp_string(new.clock),
                    str(new.bpm),
                    "true" if changed else "false",
                )
            )
            + "},"
        )
    lines.extend(("};", ""))

    lines.extend(emit_byte_matrix("kDrums", [row.drums for row in new_rows], "kDrumBytes"))
    lines.extend(
        emit_byte_matrix("kOldSynthA", [row.synth_a for row in old_rows], "kSynthBytes")
    )
    lines.extend(
        emit_byte_matrix("kOldSynthB", [row.synth_b for row in old_rows], "kSynthBytes")
    )
    lines.extend(
        emit_byte_matrix("kNewSynthA", [row.synth_a for row in new_rows], "kSynthBytes")
    )
    lines.extend(
        emit_byte_matrix("kNewSynthB", [row.synth_b for row in new_rows], "kSynthBytes")
    )
    lines.extend(
        (
            "}  // namespace F08ListenFixtureData",
            "}  // namespace GroovePuterRhythm",
            "",
        )
    )
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate the test-only F08 OLD/NEW hardware listening fixture."
    )
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    migration = ROOT / "src/generation/migration/strong_rhythm_migration.cpp"
    current_source = migration.read_text(encoding="utf-8")
    legacy_source = reverse_f08_source(current_source)

    with tempfile.TemporaryDirectory(prefix="grooveputer-f08-listen-") as temp_name:
        temp = Path(temp_name)
        legacy_cpp = temp / "strong_rhythm_migration_legacy.cpp"
        legacy_cpp.write_text(legacy_source, encoding="utf-8")

        old_exe = temp / "f08_listen_old"
        new_exe = temp / "f08_listen_new"
        compile_dump(legacy_cpp, old_exe)
        compile_dump(migration, new_exe)

        old_rows = run_dump(old_exe, "--legacy")
        new_rows = run_dump(new_exe, "--independent")
        validate(old_rows, new_rows)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(generate_header(old_rows, new_rows), encoding="utf-8")
    print(
        f"F08 LISTEN fixture: OK cases={len(CASES)} "
        f"output={args.output}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
