#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(text: str, needle: str, source: str) -> None:
    assert needle in text, f"missing {needle!r} in {source}"


def main() -> None:
    genre_h = read("src/dsp/genre_manager.h")
    genre_cpp = read("src/dsp/genre_manager.cpp")
    rhythm_h = read("src/generation/rhythm/reference_vocabulary.h")
    rhythm_cpp = read("src/generation/rhythm/reference_vocabulary.cpp")
    selection = read("src/generation/composition/rhythm_selection.cpp")
    profile_h = read("src/generation/composition/generation_profile.h")
    profile_cpp = read("src/generation/composition/generation_profile.cpp")
    migration_h = read("src/generation/migration/strong_rhythm_migration.h")
    migration_cpp = read("src/generation/migration/strong_rhythm_migration.cpp")
    ui = read("src/ui/pages/genre_page.cpp")
    scenes = read("scenes.cpp")
    backlog = read("docs/architecture/GENERATION_NIGHTLY_BASELINE_AND_BACKLOG.md")
    stage12 = read("docs/architecture/GROOVE_VOCABULARY_STAGE12_ACCEPTANCE.md")
    atlas = read("docs/architecture/ATLAS_PASS1_CANDIDATES.csv")

    # Persisted Genre IDs 0..8 are frozen; Stage 14 is append-only.
    frozen = [
        "Acid = 0", "Outrun = 1", "Darksynth = 2", "Electro = 3",
        "Rave = 4", "Reggae = 5", "TripHop = 6", "Broken = 7",
        "Chip = 8",
    ]
    for item in frozen:
        require(genre_h, item, "genre_manager.h")
    for item in [
        "House = 9", "Techno = 10", "HipHop = 11", "FunkSoul = 12",
        "UkGarage = 13", "DrumAndBass = 14", "LoFi = 15",
        "kGenerativeModeCount = 16",
    ]:
        require(genre_h, item, "genre_manager.h")
    assert "Latin =" not in genre_h

    for name in [
        '"House"', '"Techno"', '"Hip-Hop"', '"Funk/Soul"',
        '"UK Garage"', '"Drum&Bass"', '"Lo-Fi"',
        '"Classic Chill"', '"Drunken Groove"', '"Lo-Fi House"',
        '"Minimal Sleep"', '"Golden Era"', '"Dusty Jazz"',
    ]:
        require(genre_cpp, name, "genre_manager.cpp")

    # Production admission is evidence-gated. Stage 14 must leave the 24-entry
    # reference catalog untouched until HARD_02/HARD_04/HARD_05 have a recorded
    # repository hardware verdict.
    require(rhythm_cpp, "== 24", "reference_vocabulary.cpp")
    for token in [
        "StaggeredMachine", "CrossCycle", "BreakHalfstep",
        "archetype(701", "archetype(702", "archetype(703",
        '701, "staggered_machine"', '702, "cross_cycle"',
        '703, "break_halfstep"',
    ]:
        assert token not in rhythm_h + rhythm_cpp, token
    for token in [
        "701", "702", "703", "StaggeredMachine", "CrossCycle", "BreakHalfstep",
    ]:
        assert token not in selection, (
            f"pending Stage 7A identity leaked into production routing: {token}"
        )
    require(
        backlog,
        "Missing evidence means `HARDWARE_PENDING`, never inferred acceptance.",
        "GENERATION_NIGHTLY_BASELINE_AND_BACKLOG.md",
    )

    # Atlas Lo-Fi/Boom-Bap research rows remain REVIEW rather than becoming
    # genre-named production topology.
    for line in atlas.splitlines():
        if line.startswith("lofi_sparse_stable_backbeat,") or line.startswith(
            "boombap_syncopated_kick_backbeat,"
        ):
            assert ",REVIEW," in line, line
    assert "LoFiGenerator" not in profile_cpp + selection + migration_cpp + ui
    assert "BoomBapGenerator" not in profile_cpp + selection + migration_cpp + ui

    # Lo-Fi target uses one physical Synth B with chord priority plus sparse
    # melodic fills, not a fictitious third voice.
    require(profile_h, "ChordWithMelodicFill", "generation_profile.h")
    require(migration_h, "ChordWithMelodicFill", "strong_rhythm_migration.h")
    require(migration_cpp, "chordOccupied", "strong_rhythm_migration.cpp")
    require(
        migration_cpp,
        "admittedMelodicContinuations",
        "strong_rhythm_migration.cpp",
    )
    require(migration_cpp, "melodicFillOnsets", "strong_rhythm_migration.cpp")
    require(migration_cpp, "semanticBarOrdinal", "strong_rhythm_migration.cpp")
    require(
        migration_cpp,
        "melodicRequest.barOrdinal = barOrdinal;",
        "strong_rhythm_migration.cpp",
    )
    for dense_id in [
        "MelodicRhythmId::SyncopatedMotif",
        "MelodicRhythmId::RepeatedCell",
    ]:
        lofi_start = profile_cpp.index(
            "constexpr WeightedIdentityCandidate kMelodicLoFi[]"
        )
        lofi_end = profile_cpp.index("};", lofi_start)
        assert dense_id not in profile_cpp[lofi_start:lofi_end], dense_id

    # Stage 12 remains intentionally production-blocked. Stage 14 may select and
    # report phrase planning metadata but must not wire a forbidden multi-bar caller.
    require(
        stage12,
        "no production caller exists yet",
        "GROOVE_VOCABULARY_STAGE12_ACCEPTANCE.md",
    )
    require(
        stage12,
        "BLOCKED_BY_STAGE_6_1_HARDWARE_GATE",
        "GROOVE_VOCABULARY_STAGE12_ACCEPTANCE.md",
    )
    require(migration_cpp, "request.phraseBars = 1;", "strong_rhythm_migration.cpp")
    require(profile_h, "Planning metadata only", "generation_profile.h")

    # Variant-specific BPM is owned by the selected generation corridor.
    require(ui, "generationProfileFor(settings)", "genre_page.cpp")
    require(ui, "profile.corridor.suggestedBpm", "genre_page.cpp")
    require(ui, "selectedProfile.corridor.bpmMin", "genre_page.cpp")
    require(ui, "selectedProfile.corridor.bpmMax", "genre_page.cpp")
    assert "kGenreBpm" not in ui

    # Preserve the full-host-suite source contract that the previous revision broke.
    require(
        ui,
        "// ENTER: apply the current genre/recipe selection.",
        "genre_page.cpp",
    )

    # New persisted Genre values use the dynamic decoder bound; existing Rhythm
    # selection intent remains persisted by the unchanged Scene codec.
    assert scenes.count("if (gen >= kGenerativeModeCount) gen = 0;") >= 1
    assert scenes.count("if (v >= kGenerativeModeCount) v = 0;") >= 1
    for persisted_key in ['["gen"]', '["rcp"]', '["rsm"]', '["rid"]']:
        require(scenes, persisted_key, "scenes.cpp")

    print("Generation Stage 14 source regressions: OK")


if __name__ == "__main__":
    main()
