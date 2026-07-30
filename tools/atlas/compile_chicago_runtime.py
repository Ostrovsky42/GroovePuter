#!/usr/bin/env python3
"""Compile the Chicago Jack Atlas v2.6 vertical slice for GroovePuter."""

from __future__ import annotations

import argparse
import csv
import hashlib
import io
import json
import re
import zipfile
from collections import defaultdict
from pathlib import Path

EXPECTED_SHA256 = "5b155937b8d05f0f0f9f1a02f10d9afe76a917d6035897695cce739eb8d6b1fd"
RECIPE_ID = "REC_ACID_CHICAGO_JACK"
RUNTIME_ID = 6
TARGETS = {
    "KICK": 0,
    "SNARE": 1,
    "HAT1": 2,
    "HAT2": 3,
    "PERC1": 4,
    "PERC2": 5,
    "RIM": 6,
    "CLAP": 7,
    "SYNTH1": 8,
    "SYNTH2": 9,
    "DX": 9,
}
PRIORITY = {"SYNTH2": 20, "DX": 10}
ROOTS = {
    "C": 0,
    "C#": 1,
    "DB": 1,
    "D": 2,
    "D#": 3,
    "EB": 3,
    "E": 4,
    "F": 5,
    "F#": 6,
    "GB": 6,
    "G": 7,
    "G#": 8,
    "AB": 8,
    "A": 9,
    "A#": 10,
    "BB": 10,
    "B": 11,
}
ACTIVE, ACCENT, SLIDE, SUSTAIN = 1, 2, 4, 8


def rows(archive: zipfile.ZipFile, root: str, relative_path: str):
    text = archive.read(root + relative_path).decode("utf-8-sig")
    return list(csv.DictReader(io.StringIO(text)))


def integer(value: str | None, default: int = 0) -> int:
    normalized = (value or "").strip()
    return int(normalized) if normalized else default


def boolean(value: str | None) -> bool:
    return (value or "").strip().lower() == "true"


def clamp(value: int, low: int, high: int) -> int:
    return max(low, min(high, value))


def identifier(value: str) -> str:
    return re.sub(r"[^A-Za-z0-9]+", "_", value).strip("_")


def quoted(value: str) -> str:
    return json.dumps(value, ensure_ascii=True)


def chord_note(event: dict[str, str]) -> int | None:
    root = (event.get("chord_root") or "").strip()
    if not root:
        match = re.match(r"^([A-Ga-g])([#b]?)", (event.get("pitch") or "").strip())
        root = match.group(1).upper() + match.group(2) if match else ""
    pitch_class = ROOTS.get(root.upper())
    return None if pitch_class is None else 48 + pitch_class


def compile_event(event: dict[str, str], track: str):
    target = TARGETS.get(track)
    if target is None:
        return None

    step = integer(event.get("step_index")) - 1
    if not 0 <= step < 16:
        raise ValueError(f"{event['event_id']}: invalid step")

    timing = clamp(
        integer(event.get("substep_offset"))
        + integer(event.get("microtiming_ticks")),
        -23,
        23,
    )
    velocity = clamp(integer(event.get("velocity"), 100), 1, 127)
    probability = clamp(integer(event.get("probability_percent"), 100), 0, 100)
    articulation = (event.get("articulation") or "").strip().upper()

    flags = ACTIVE
    if boolean(event.get("accent")) or articulation == "ACCENT":
        flags |= ACCENT
    if boolean(event.get("glide_from_previous")) or articulation == "SLIDE":
        flags |= SLIDE
    if articulation == "SUSTAIN" or integer(event.get("note_length_steps"), 1) > 1:
        flags |= SUSTAIN

    if target >= 8:
        raw_note = (event.get("midi_note") or "").strip()
        note = (
            clamp(int(raw_note), 0, 127)
            if raw_note
            else (chord_note(event) if track == "SYNTH2" else None)
        )
        if note is None:
            return None
    else:
        note = -1

    return (
        target,
        step,
        note,
        velocity,
        timing,
        probability,
        flags,
        PRIORITY.get(track, 0),
    )


def render_types() -> str:
    return '''#pragma once

#include <cstddef>
#include <cstdint>

// Generated from SEQTRAK Pattern Atlas schema 2.6.0.
// Do not edit manually.

namespace AtlasGenerated {

enum EventFlags : uint8_t {
  kActive = 1u << 0,
  kAccent = 1u << 1,
  kSlide = 1u << 2,
  kSustain = 1u << 3,
};

struct Event {
  uint8_t target;
  uint8_t step;
  int8_t note;
  uint8_t velocity;
  int8_t timing;
  uint8_t probability;
  uint8_t flags;
};

struct Pattern {
  const char* atlasPatternId;
  const char* slotId;
  const char* slotFunction;
  const Event* events;
  uint16_t eventCount;
};

struct Recipe {
  uint8_t runtimeRecipeId;
  const char* atlasRecipeId;
  const char* displayName;
  uint16_t bpm;
  uint8_t swingPercent;
  const Pattern* patterns;
  uint8_t patternCount;
};

}  // namespace AtlasGenerated
'''


def render_recipe(
    recipe: dict[str, str],
    links: list[dict[str, str]],
    pattern_data: dict[str, list[tuple[int, ...]]],
) -> str:
    lines = [
        "#pragma once",
        "",
        '#include "atlas_runtime_types.generated.h"',
        "",
        "namespace AtlasGenerated {",
        "",
    ]
    arrays: list[tuple[dict[str, str], str]] = []

    for link in links:
        pattern_id = link["pattern_id"]
        name = "kEvents_" + identifier(pattern_id)
        arrays.append((link, name))
        lines.append(f"inline constexpr Event {name}[] = {{")
        for event in pattern_data[pattern_id]:
            lines.append(
                f"  {{{event[0]}, {event[1]}, {event[2]}, {event[3]}, "
                f"{event[4]}, {event[5]}, {event[6]}}},"
            )
        lines.extend(["};", ""])

    patterns_name = "kPatterns_" + identifier(RECIPE_ID)
    lines.append(f"inline constexpr Pattern {patterns_name}[] = {{")
    for link, event_name in arrays:
        lines.append(
            f"  {{{quoted(link['pattern_id'])}, {quoted(link['slot_id'])}, "
            f"{quoted(link['slot_function'])}, {event_name}, "
            f"static_cast<uint16_t>(sizeof({event_name}) / "
            f"sizeof({event_name}[0]))}},"
        )

    display_name = recipe["display_name"].replace(" SEQTRAK recipe", "")
    lines.extend(
        [
            "};",
            "",
            f"inline constexpr Recipe kRecipe_{identifier(RECIPE_ID)} = "
            f"{{{RUNTIME_ID}, {quoted(RECIPE_ID)}, {quoted(display_name)}, "
            f"{integer(recipe['default_bpm'])}, {integer(recipe['swing_percent'])}, "
            f"{patterns_name}, static_cast<uint8_t>(sizeof({patterns_name}) / "
            f"sizeof({patterns_name}[0]))}};",
            "",
            "}  // namespace AtlasGenerated",
            "",
        ]
    )
    return "\n".join(lines)


def render_index(ignored_sampler_events: int) -> str:
    return f'''#pragma once

#include "atlas_runtime_types.generated.h"
#include "rec_acid_chicago_jack.generated.h"

namespace AtlasGenerated {{

inline constexpr Recipe kRecipes[] = {{
  kRecipe_REC_ACID_CHICAGO_JACK,
}};

inline constexpr size_t kRecipeCount = sizeof(kRecipes) / sizeof(kRecipes[0]);
inline constexpr uint16_t kIgnoredSamplerEvents = {ignored_sampler_events};
inline constexpr uint16_t kIgnoredUnsupportedTracks = 0;
inline constexpr uint16_t kIgnoredUnrepresentablePitchEvents = 0;

}}  // namespace AtlasGenerated
'''


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("atlas_zip", type=Path)
    parser.add_argument("output_dir", type=Path)
    args = parser.parse_args()

    digest = hashlib.sha256(args.atlas_zip.read_bytes()).hexdigest()
    if digest != EXPECTED_SHA256:
        raise ValueError(f"unexpected Atlas archive SHA-256: {digest}")

    with zipfile.ZipFile(args.atlas_zip) as archive:
        roots = {name.split("/", 1)[0] for name in archive.namelist() if "/" in name}
        if len(roots) != 1:
            raise ValueError("Atlas ZIP must contain one root")
        root = next(iter(roots)) + "/"

        summary = json.loads(archive.read(root + "reports/validation_summary.json"))
        if summary.get("schema_version") != "2.6.0" or summary.get("failures") != 0:
            raise ValueError("Atlas validation gate failed")

        recipes = rows(archive, root, "core/recipes.csv")
        capabilities = rows(archive, root, "runtime/recipe_application_capabilities.csv")
        links = rows(archive, root, "core/recipe_patterns.csv")
        patterns = rows(archive, root, "core/patterns.csv")
        tracks = rows(archive, root, "core/pattern_tracks.csv")
        events = rows(archive, root, "core/pattern_events.csv")

    recipe = next(row for row in recipes if row["recipe_id"] == RECIPE_ID)
    capability = next(row for row in capabilities if row["recipe_id"] == RECIPE_ID)
    if not recipe["publication_status"].startswith("PUBLISHED"):
        raise ValueError("recipe is not published")
    if not boolean(capability["can_apply_pattern_to_internal_project"]):
        raise ValueError("recipe is not runtime-applicable")

    links = sorted(
        (row for row in links if row["recipe_id"] == RECIPE_ID),
        key=lambda row: integer(row["slot_order"]),
    )
    if [row["slot_id"] for row in links] != ["P1", "P2", "P3"]:
        raise ValueError("Chicago Jack must contain P1/P2/P3")

    pattern_map = {row["pattern_id"]: row for row in patterns}
    tracks_by_pattern: dict[str, list[dict[str, str]]] = defaultdict(list)
    events_by_track: dict[tuple[str, str], list[dict[str, str]]] = defaultdict(list)
    for row in tracks:
        tracks_by_pattern[row["pattern_id"]].append(row)
    for row in events:
        events_by_track[(row["pattern_id"], row["track_id"])].append(row)

    compiled: dict[str, list[tuple[int, ...]]] = {}
    ignored_sampler_events = 0

    for link in links:
        pattern_id = link["pattern_id"]
        pattern = pattern_map[pattern_id]
        if integer(pattern["bars"]) != 1 or integer(pattern["steps_per_bar"]) != 16:
            raise ValueError(f"unsupported pattern shape: {pattern_id}")
        if not pattern["publication_status"].startswith("PUBLISHED"):
            raise ValueError(f"pattern is not published: {pattern_id}")

        merged: dict[tuple[int, int], tuple[int, ...]] = {}
        ordered_tracks = sorted(
            tracks_by_pattern[pattern_id],
            key=lambda row: integer(row["track_order"]),
        )
        for track_row in ordered_tracks:
            track = track_row["track_id"]
            track_events = events_by_track[(pattern_id, track)]
            if track == "SAMPLER":
                ignored_sampler_events += len(track_events)
                continue

            for raw_event in track_events:
                event = compile_event(raw_event, track)
                if event is None:
                    continue
                key = (event[0], event[1])
                previous = merged.get(key)
                if previous is None or event[7] >= previous[7]:
                    merged[key] = event

        compiled[pattern_id] = sorted(
            merged.values(), key=lambda event: (event[0], event[1])
        )

    args.output_dir.mkdir(parents=True, exist_ok=True)
    (args.output_dir / "atlas_runtime_types.generated.h").write_text(
        render_types(), encoding="utf-8"
    )
    (args.output_dir / "rec_acid_chicago_jack.generated.h").write_text(
        render_recipe(recipe, links, compiled), encoding="utf-8"
    )
    (args.output_dir / "atlas_runtime.generated.h").write_text(
        render_index(ignored_sampler_events), encoding="utf-8"
    )

    total_events = sum(len(pattern_events) for pattern_events in compiled.values())
    print(
        f"compiled {RECIPE_ID}: {total_events} runtime events, "
        f"{ignored_sampler_events} sampler events ignored"
    )


if __name__ == "__main__":
    main()
