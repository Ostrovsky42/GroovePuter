#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = ROOT / "tools/research/harmony_atlas_h1.py"
spec = importlib.util.spec_from_file_location("harmony_atlas_h1", MODULE_PATH)
if spec is None or spec.loader is None:
    raise RuntimeError("cannot load Harmony Atlas H1 module")
h1 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(h1)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def token(value: str):
    return h1.normalize_chord_token(value)


def test_degree_and_accidental() -> None:
    require(token("I")["root"]["diatonic_degree"] == 0, "I degree")
    require(token("bIII")["root"]["diatonic_degree"] == 2, "bIII degree")
    require(token("bIII")["root"]["alteration_semitones"] == -1, "bIII flat")
    require(token("#IVm")["root"]["diatonic_degree"] == 3, "#IV degree")
    require(token("#IVm")["root"]["alteration_semitones"] == 1, "#IV sharp")


def test_quality_is_loss_aware() -> None:
    require(token("I")["quality"]["triad_class"] == "MAJOR", "uppercase major")
    require(token("ii")["quality"]["triad_class"] == "MINOR", "lowercase minor")
    require(token("bIIIM")["quality"]["triad_class"] == "MAJOR", "explicit M")
    require(token("im")["quality"]["triad_class"] == "MINOR", "explicit m")
    require(token("vdim")["quality"]["triad_class"] == "DIMINISHED", "dim")
    require(token("Isus2")["quality"]["triad_class"] == "SUSPENDED_2", "sus2")
    require(token("Isus4")["quality"]["triad_class"] == "SUSPENDED_4", "sus4")
    require(token("I5")["quality"]["triad_class"] == "POWER_5", "power5")

    generic7 = token("I7")["quality"]
    dom7 = token("Idom7")["quality"]
    maj7 = token("IM7")["quality"]
    require(generic7["seventh_flavor"] == "UNSPECIFIED", "generic 7")
    require(dom7["seventh_flavor"] == "DOMINANT", "dom7")
    require(maj7["seventh_flavor"] == "MAJOR", "M7")
    require(generic7 != dom7 and generic7 != maj7, "7 flavors collapsed")
    require(token("IM-5")["quality"]["fifth_alteration_semitones"] == -1, "M-5")


def test_round_trip() -> None:
    samples = [
        "I", "ii", "bIII", "#IVm", "I7", "Idom7", "IM7", "im7",
        "I69", "viadd9", "imadd9", "Vsus4", "vdim",
    ]
    for source in samples:
        normalized = token(source)
        require(h1.render_source_token(normalized) == source, f"round trip: {source}")


def test_unknown_suffix_rejected() -> None:
    try:
        token("Imaj7")
    except h1.NormalizationError as exc:
        require(exc.code == "UNSUPPORTED_SUFFIX", f"wrong error: {exc.code}")
    else:
        raise AssertionError("unknown suffix must be rejected")


def test_descriptor_typing() -> None:
    typed, unknown = h1.type_descriptors(["Hopeful", "Cadence", "New"])
    require(typed == {"mood": ["Hopeful"], "structural": ["Cadence"], "catalog": ["New"]}, "tag typing")
    require(unknown == [], "known descriptor reported unknown")
    _, unknown = h1.type_descriptors(["UnreviewedLabel"])
    require(unknown == ["UnreviewedLabel"], "unknown descriptor lost")


def write_fixture(root: Path, *, bad_token: bool = False, unknown_tag: bool = False) -> None:
    first = "Imaj7 V" if bad_token else "I V I I"
    tag = "UnreviewedLabel" if unknown_tag else "Hopeful"
    chords = (
        "prog_maj = [\n"
        f"    \"{first} ={tag}\",\n"
        "    \"I  V =Cadence New\",\n"
        "]\n"
        "prog_min = []\n"
        "prog_modal = []\n"
        "chord_types_maj = [\"sus2\"]\n"
        "chord_types_min = [\"m7\"]\n"
    )
    (root / "README.md").write_text("fixture\n", encoding="utf-8")
    (root / "chords.py").write_text(chords, encoding="utf-8")
    (root / "gen.py").write_text('keys = [("C", "A")]\nstyles = [""]\n', encoding="utf-8")


def test_definition_order_repetition_rest_and_contract() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        write_fixture(root)
        result = h1.build_normalization(root, verify_pin=False)
    require(result["summary"]["logical_definition_count"] == 2, "logical count")
    require(result["summary"]["admitted_definition_count"] == 2, "admitted count")
    require(result["summary"]["quarantined_definition_count"] == 0, "unexpected quarantine")
    require(result["summary"]["normalized_rest_event_count"] == 1, "rest not preserved")
    first = result["definitions"][0]
    require(first["event_refs"][0] == first["event_refs"][2] == first["event_refs"][3], "repetition/order lost")
    require("REST" in result["definitions"][1]["event_refs"], "REST ref missing")
    contract = result["normalization_contract"]
    require(contract["progression_deduplication"] == "NOT_PERFORMED", "H1 dedup drift")
    require(contract["absolute_midi_projection"] == "FORBIDDEN", "H1 MIDI projection drift")
    require(contract["runtime_admission"] == "NOT_PERFORMED", "H1 runtime admission drift")


def test_quarantine() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        write_fixture(root, bad_token=True)
        result = h1.build_normalization(root, verify_pin=False)
    require(result["summary"]["quarantined_definition_count"] == 1, "bad token not quarantined")
    codes = {reason["code"] for reason in result["quarantine"][0]["reasons"]}
    require("UNSUPPORTED_SUFFIX" in codes, f"suffix reason missing: {codes}")

    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        write_fixture(root, unknown_tag=True)
        result = h1.build_normalization(root, verify_pin=False)
    require(result["summary"]["quarantined_definition_count"] == 1, "unknown tag not quarantined")
    codes = {reason["code"] for reason in result["quarantine"][0]["reasons"]}
    require("UNKNOWN_DESCRIPTOR" in codes, f"tag reason missing: {codes}")


def test_suffix_contract() -> None:
    expected = {
        "", "5", "6", "69", "7", "9", "M", "M-5", "M6", "M7",
        "add9", "dim", "dom7", "m", "m6", "m7", "m9", "madd9", "sus2", "sus4",
    }
    require(h1.SUPPORTED_RAW_SUFFIXES == expected, "suffix contract drift")


def main() -> None:
    test_degree_and_accidental()
    test_quality_is_loss_aware()
    test_round_trip()
    test_unknown_suffix_rejected()
    test_descriptor_typing()
    test_definition_order_repetition_rest_and_contract()
    test_quarantine()
    test_suffix_contract()
    print("Harmony Atlas H1 tests: OK")


if __name__ == "__main__":
    main()
