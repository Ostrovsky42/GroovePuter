#!/usr/bin/env python3
import csv
import hashlib
import io
import re
import subprocess
from collections import defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "build" / "host-tests" / "gf2-c1f"
OUT = BUILD / "gf2_c1f_final_static_dump"
BASE_SHA = "0a2a6211ef00dcf2214dfd4704b6c34b424b1c9d"

PROFILE_ARTIFACT = ROOT / "docs/research/GF2_C1F_FINAL_STATIC_SEMANTIC_CENSUS.tsv"
ARCHETYPE_ARTIFACT = ROOT / "docs/research/GF2_C1F_RHYTHM_ARCHETYPE_PAYLOADS.tsv"
PAIR_ARTIFACT = ROOT / "docs/research/GF2_C1F_BASE_GENRE_PAIRS.tsv"

BAGS = [
    "rhythms",
    "feels",
    "bass",
    "chord",
    "progressions",
    "melodic",
    "motifs",
    "phrases",
]

TONAL_FIELDS = [
    "bass_allowed_contours_mask",
    "bass_preferred_contours_mask",
    "bass_allowed_articulations_mask",
    "bass_preferred_articulations_mask",
    "melodic_allowed_rhythm_ops_mask",
    "melodic_preferred_rhythm_ops_mask",
    "melodic_allowed_contours_mask",
    "melodic_preferred_contours_mask",
    "melodic_allowed_motif_ops_mask",
    "melodic_preferred_motif_ops_mask",
    "bass_register_min",
    "bass_register_max",
    "bass_register_max_leap",
    "secondary_register_min",
    "secondary_register_max",
    "secondary_register_max_leap",
]

SOURCES = [
    "src/generation/generation_context.cpp",
    "src/generation/composition/rhythm_selection.cpp",
    "src/generation/composition/generation_profile.cpp",
    "src/generation/composition/phrase_length_request.cpp",
    "src/generation/composition/tonal_profile.cpp",
    "src/generation/feel/feel_interpreter.cpp",
    "src/generation/feel/feel_pattern_adapter.cpp",
    "src/generation/rhythm/rhythm_catalog.cpp",
    "src/generation/rhythm/relationship_resolver.cpp",
    "src/generation/rhythm/rhythm_realizer.cpp",
    "src/generation/rhythm/reference_vocabulary.cpp",
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
    "src/generation/migration/strong_rhythm_migration.cpp",
    "src/dsp/genre_manager.cpp",
    "scenes.cpp",
    "json_evented.cpp",
    "src/audio/pattern_paging.cpp",
    "tools/gf2/gf2_c1f_final_static_dump.cpp",
]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def sha(value: str) -> str:
    return hashlib.sha256(value.encode("utf-8")).hexdigest()


def compile_dump() -> None:
    BUILD.mkdir(parents=True, exist_ok=True)
    command = [
        "g++",
        "-std=c++17",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-Wvla",
        "-Wno-c++20-extensions",
        "-Wno-unused-variable",
        "-Wno-unused-but-set-variable",
        f"-I{ROOT}",
        f"-I{ROOT / 'platform_sdl'}",
        "-include",
        str(ROOT / "platform_sdl/arduino_compat.h"),
        *[str(ROOT / source) for source in SOURCES],
        "-o",
        str(OUT),
    ]
    subprocess.run(command, check=True, cwd=ROOT)


def dump_rows(command: str) -> list[dict[str, str]]:
    output = subprocess.run(
        [str(OUT), command],
        check=True,
        cwd=ROOT,
        text=True,
        capture_output=True,
    ).stdout
    return list(csv.DictReader(io.StringIO(output), delimiter="\t"))


def parse_candidate(value: str) -> tuple[int, str, int]:
    identity, weight = value.rsplit("@", 1)
    raw_id, name = identity.split(":", 1)
    return int(raw_id), name, int(weight)


def candidate_entries(value: str) -> list[tuple[int, str, int]]:
    if not value:
        return []
    return sorted((parse_candidate(item) for item in value.split(";")))


def support(value: str) -> str:
    return ",".join(str(item[0]) for item in candidate_entries(value))


def weighted(value: str) -> str:
    return ",".join(
        f"{item[0]}@{item[2]}" for item in candidate_entries(value)
    )


def bit_count(value: str) -> int:
    return int(value, 16).bit_count()


def tonal_sources() -> dict[str, str]:
    source = (
        ROOT / "src/generation/composition/tonal_profile.cpp"
    ).read_text(encoding="utf-8")
    rows = re.findall(
        r"row\(GenerativeMode::(\w+),\s*(k\w+Profile)\)", source
    )
    result = dict(rows)
    require(rows, "expected at least one tonal base row")
    require(
        len(result) == len(rows),
        "duplicate genre entry in tonal base rows",
    )
    return result


def add_fingerprints(
    profiles: list[dict[str, str]],
    archetypes: list[dict[str, str]],
) -> None:
    tonal_by_genre = tonal_sources()
    archetype_by_id: dict[int, dict[str, str]] = {}
    for row in archetypes:
        row["semantic_payload_fingerprint"] = sha(row["semantic_payload"])
        row["drum_payload_fingerprint"] = sha(row["drum_payload"])
        archetype_by_id[int(row["archetype_id"])] = row

    for row in profiles:
        row["exact_base"] = BASE_SHA
        row["tonal_source"] = tonal_by_genre[row["genre_key"]]
        row["composition_support"] = "|".join(
            f"{bag}={support(row[bag])}" for bag in BAGS
        )
        row["composition_weighted"] = "|".join(
            f"{bag}={weighted(row[bag])}" for bag in BAGS
        )
        row["composition_support_fingerprint"] = sha(
            row["composition_support"]
        )
        row["composition_weighted_fingerprint"] = sha(
            row["composition_weighted"]
        )
        row["corridor_payload"] = (
            f"bpm={row['bpm_min']}:{row['bpm_suggested']}:{row['bpm_max']}"
            f"|grid={row['grid_steps']}"
            f"|density={row['density_min']}:{row['density_max']}"
        )
        row["corridor_fingerprint"] = sha(row["corridor_payload"])
        row["role_fingerprint"] = sha(row["secondary_role"])
        row["tonal_payload"] = "|".join(
            f"{field}={row[field]}" for field in TONAL_FIELDS
        )
        row["tonal_payload_fingerprint"] = sha(row["tonal_payload"])

        rhythm_payloads = []
        drum_payloads = []
        for archetype_id, _, weight_value in candidate_entries(row["rhythms"]):
            archetype = archetype_by_id[archetype_id]
            rhythm_payloads.append(
                f"{weight_value}@{archetype['semantic_payload_fingerprint']}"
            )
            drum_payloads.append(
                f"{weight_value}@{archetype['drum_payload_fingerprint']}"
            )
        row["rhythm_payload_fingerprint"] = sha("|".join(rhythm_payloads))
        # This is the genre/recipe-owned drum projection only.  The Scene FEEL
        # selection is an independent runtime owner and must not be folded into
        # a new genre drum semantic owner here.
        row["canonical_drum_payload"] = f"rhythms={'|'.join(drum_payloads)}"
        row["canonical_drum_fingerprint"] = sha(
            row["canonical_drum_payload"]
        )
        row["legacy_params_fingerprint"] = sha(row["legacy_params"])
        row["legacy_drum_fingerprint"] = sha(row["legacy_drum_template"])
        row["legacy_behavior_fingerprint"] = sha(row["legacy_behavior"])
        row["legacy_timbre_fingerprint"] = sha(row["legacy_timbre"])
        row["atlas_metadata_payload"] = "|".join([
            f"backed={row['atlas_backed']}",
            f"recipe={row['atlas_recipe_id']}",
            f"display={row['atlas_display_name']}",
            f"bpm={row['atlas_bpm']}",
            f"swing={row['atlas_swing_percent']}",
            f"variations={row['atlas_variation_count']}",
        ])
        row["atlas_metadata_fingerprint"] = sha(
            row["atlas_metadata_payload"]
        )
        if row["atlas_backed"] == "0":
            row["atlas_corridor_relation"] = "NOT_ATLAS_BACKED"
        else:
            atlas_bpm = int(row["atlas_bpm"])
            if atlas_bpm == int(row["bpm_suggested"]):
                row["atlas_corridor_relation"] = "MATCHES_SUGGESTED_BPM"
            elif int(row["bpm_min"]) <= atlas_bpm <= int(row["bpm_max"]):
                row["atlas_corridor_relation"] = "INSIDE_CORRIDOR"
            else:
                row["atlas_corridor_relation"] = "OUTSIDE_CORRIDOR"
        row["primary_static_fingerprint"] = sha(
            "|".join(
                [
                    row["composition_weighted_fingerprint"],
                    row["corridor_fingerprint"],
                    row["role_fingerprint"],
                    row["tonal_payload_fingerprint"],
                    row["canonical_drum_fingerprint"],
                ]
            )
        )
        row["full_trace_fingerprint"] = sha(
            "|".join(
                [
                    row["primary_static_fingerprint"],
                    row["legacy_params_fingerprint"],
                    row["legacy_drum_fingerprint"],
                    row["legacy_behavior_fingerprint"],
                    row["legacy_timbre_fingerprint"],
                    row["atlas_metadata_fingerprint"],
                ]
            )
        )

        singles = []
        for bag in BAGS:
            entries = candidate_entries(row[bag])
            if len(entries) == 1:
                singles.append(f"composition.{bag}={entries[0][1]}")
        tonal_single_fields = [
            ("tonal.bass_contour", "bass_allowed_contours_mask",
             "bass_allowed_contours"),
            ("tonal.bass_articulation", "bass_allowed_articulations_mask",
             "bass_allowed_articulations"),
            ("tonal.melodic_rhythm_operation",
             "melodic_allowed_rhythm_ops_mask", "melodic_allowed_rhythm_ops"),
            ("tonal.melodic_contour", "melodic_allowed_contours_mask",
             "melodic_allowed_contours"),
            ("tonal.melodic_motif_operation",
             "melodic_allowed_motif_ops_mask", "melodic_allowed_motif_ops"),
        ]
        for axis, mask_field, name_field in tonal_single_fields:
            if bit_count(row[mask_field]) == 1:
                singles.append(f"{axis}={row[name_field]}")
        row["single_option_axes"] = ";".join(singles) or "NONE"


def classify_recipes(profiles: list[dict[str, str]]) -> None:
    base_rows = [row for row in profiles if row["is_base"] == "1"]
    bases = {
        row["genre_key"]: row for row in base_rows
    }
    require(base_rows, "expected at least one BASE row")
    require(
        len(bases) == len(base_rows),
        "duplicate genre key in BASE rows",
    )

    for row in profiles:
        if row["is_base"] == "1":
            row["classification_vs_base"] = "BASE"
            row["changed_domains_vs_base"] = "NONE"
            row["legacy_trace_changes_vs_base"] = "NONE"
            row["runtime_trace_changes_vs_base"] = "NONE"
            continue

        require(
            row["genre_key"] in bases,
            f"recipe has no BASE row: {row['genre_key']}/{row['recipe_name']}",
        )
        base = bases[row["genre_key"]]
        membership_changed = any(
            support(row[bag]) != support(base[bag]) for bag in BAGS
        )
        composition_changed = (
            row["composition_weighted_fingerprint"]
            != base["composition_weighted_fingerprint"]
        )
        corridor_changed = (
            row["corridor_fingerprint"] != base["corridor_fingerprint"]
        )
        role_changed = row["role_fingerprint"] != base["role_fingerprint"]
        tonal_changed = (
            row["tonal_payload_fingerprint"]
            != base["tonal_payload_fingerprint"]
        )
        # The production full-generation drum path is the strong-rhythm
        # vocabulary projection.  Legacy templates are retained separately as
        # traceability data, not substituted for the effective drum domain.
        drum_changed = (
            row["canonical_drum_fingerprint"]
            != base["canonical_drum_fingerprint"]
        )

        domains = []
        if composition_changed:
            domains.append("COMPOSITION")
        if corridor_changed:
            domains.append("CORRIDOR")
        if role_changed:
            domains.append("ROLE")
        if tonal_changed:
            domains.append("TONAL")
        if drum_changed:
            domains.append("DRUM")

        labels = []
        if not domains:
            labels.append("BASE-EQUIVALENT")
        if membership_changed:
            labels.append("MEMBERSHIP-CHANGE")
        if (composition_changed and not membership_changed and
                domains == ["COMPOSITION"]):
            labels.append("WEIGHTS-ONLY")
        if domains == ["CORRIDOR"]:
            labels.append("CORRIDOR-ONLY")
        if role_changed:
            labels.append("ROLE-CHANGE")
        if domains == ["TONAL"]:
            labels.append("TONAL-ONLY")
        if domains == ["DRUM"]:
            labels.append("DRUM-ONLY")
        if len(domains) > 1:
            labels.append("MULTI-DOMAIN")
        if not labels:
            labels.append("MULTI-DOMAIN" if len(domains) > 1
                          else "MEMBERSHIP-CHANGE")

        legacy_changes = []
        for name, field in (
            ("PARAMS", "legacy_params_fingerprint"),
            ("DRUM", "legacy_drum_fingerprint"),
            ("BEHAVIOR", "legacy_behavior_fingerprint"),
            ("TIMBRE", "legacy_timbre_fingerprint"),
        ):
            if row[field] != base[field]:
                legacy_changes.append(name)

        row["classification_vs_base"] = ";".join(labels)
        row["changed_domains_vs_base"] = ";".join(domains) or "NONE"
        row["legacy_trace_changes_vs_base"] = (
            ";".join(legacy_changes) or "NONE"
        )
        row["runtime_trace_changes_vs_base"] = (
            "ATLAS_METADATA"
            if row["atlas_metadata_fingerprint"]
            != base["atlas_metadata_fingerprint"]
            else "NONE"
        )


def base_pairs(
    profiles: list[dict[str, str]], expected_count: int | None = 120
) -> list[dict[str, str]]:
    bases = [row for row in profiles if row["is_base"] == "1"]
    pairs = []
    for left_index, left in enumerate(bases):
        for right in bases[left_index + 1:]:
            same_rhythm = weighted(left["rhythms"]) == weighted(right["rhythms"])
            same_other_composition = all(
                weighted(left[bag]) == weighted(right[bag])
                for bag in BAGS if bag != "rhythms"
            )
            same_corridor = (
                left["corridor_fingerprint"] == right["corridor_fingerprint"]
            )
            same_role = left["role_fingerprint"] == right["role_fingerprint"]
            same_tonal = (
                left["tonal_payload_fingerprint"]
                == right["tonal_payload_fingerprint"]
            )
            same_canonical_drum = (
                left["canonical_drum_fingerprint"]
                == right["canonical_drum_fingerprint"]
            )
            same_legacy_drum = (
                left["legacy_drum_fingerprint"]
                == right["legacy_drum_fingerprint"]
            )
            changed = []
            for name, same in (
                ("RHYTHM", same_rhythm),
                ("COMPOSITION_OTHER", same_other_composition),
                ("CORRIDOR", same_corridor),
                ("ROLE", same_role),
                ("TONAL", same_tonal),
                ("LEGACY_DRUM", same_legacy_drum),
            ):
                if not same:
                    changed.append(name)

            exact = left["primary_static_fingerprint"] == right[
                "primary_static_fingerprint"
            ]
            if exact:
                classification = "EXACT_DECLARATIVE_COLLISION"
            elif same_tonal or same_rhythm or same_other_composition:
                classification = "PARTIAL_COLLISION"
            else:
                classification = "STRONG_DISTINCTNESS"

            pairs.append({
                "exact_base": BASE_SHA,
                "genre_a": left["genre_key"],
                "genre_b": right["genre_key"],
                "classification": classification,
                "changed_domains": ";".join(changed) or "NONE",
                "same_rhythm_weights": str(int(same_rhythm)),
                "same_other_composition_weights": str(
                    int(same_other_composition)
                ),
                "same_corridor": str(int(same_corridor)),
                "same_secondary_role": str(int(same_role)),
                "same_tonal_payload": str(int(same_tonal)),
                "same_canonical_drum_payload": str(
                    int(same_canonical_drum)
                ),
                "same_legacy_drum_payload": str(int(same_legacy_drum)),
            })
    if expected_count is not None:
        require(
            len(pairs) == expected_count,
            f"expected {expected_count} BASE pairs, found {len(pairs)}",
        )
    return pairs


def write_tsv(path: Path, rows: list[dict[str, str]], fields: list[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=fields,
            delimiter="\t",
            lineterminator="\n",
            extrasaction="ignore",
        )
        writer.writeheader()
        writer.writerows(rows)


def main() -> None:
    compile_dump()
    profiles = dump_rows("profiles")
    archetypes = dump_rows("archetypes")
    require(len(profiles) == 33, f"expected 33 profiles, found {len(profiles)}")
    require(len(archetypes) == 24,
            f"expected 24 archetypes, found {len(archetypes)}")

    add_fingerprints(profiles, archetypes)
    classify_recipes(profiles)
    pairs = base_pairs(profiles)

    derived_profile_fields = [
        "exact_base",
        "tonal_source",
        "composition_support",
        "composition_weighted",
        "composition_support_fingerprint",
        "composition_weighted_fingerprint",
        "corridor_payload",
        "corridor_fingerprint",
        "role_fingerprint",
        "tonal_payload",
        "tonal_payload_fingerprint",
        "rhythm_payload_fingerprint",
        "canonical_drum_payload",
        "canonical_drum_fingerprint",
        "legacy_params_fingerprint",
        "legacy_drum_fingerprint",
        "legacy_behavior_fingerprint",
        "legacy_timbre_fingerprint",
        "atlas_metadata_payload",
        "atlas_metadata_fingerprint",
        "atlas_corridor_relation",
        "primary_static_fingerprint",
        "full_trace_fingerprint",
        "classification_vs_base",
        "changed_domains_vs_base",
        "legacy_trace_changes_vs_base",
        "runtime_trace_changes_vs_base",
        "single_option_axes",
    ]
    profile_fields = list(profiles[0].keys())
    profile_fields = [
        field for field in profile_fields if field not in derived_profile_fields
    ] + derived_profile_fields

    archetype_fields = list(archetypes[0].keys())
    for field in (
        "semantic_payload_fingerprint",
        "drum_payload_fingerprint",
    ):
        if field not in archetype_fields:
            archetype_fields.append(field)

    write_tsv(PROFILE_ARTIFACT, profiles, profile_fields)
    write_tsv(ARCHETYPE_ARTIFACT, archetypes, archetype_fields)
    write_tsv(PAIR_ARTIFACT, pairs, list(pairs[0].keys()))

    print(f"profiles={len(profiles)}")
    print(f"archetypes={len(archetypes)}")
    print(f"base_pairs={len(pairs)}")
    print(PROFILE_ARTIFACT.relative_to(ROOT))
    print(ARCHETYPE_ARTIFACT.relative_to(ROOT))
    print(PAIR_ARTIFACT.relative_to(ROOT))


if __name__ == "__main__":
    main()
