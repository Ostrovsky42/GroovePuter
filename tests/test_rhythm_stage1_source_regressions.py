from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TYPES = ROOT / "src/generation/rhythm/rhythm_types.h"
CATALOG_H = ROOT / "src/generation/rhythm/rhythm_catalog.h"
CATALOG_CPP = ROOT / "src/generation/rhythm/rhythm_catalog.cpp"
CORE_WORKFLOW = ROOT / ".github/workflows/core-regressions.yml"
DEDICATED_WORKFLOW = ROOT / ".github/workflows/groove-vocabulary-stage1.yml"
ATLAS_COMPILER = ROOT / "tools/atlas/compile_atlas_runtime.py"
ATLAS_TEST = ROOT / "tests/test_rhythm_atlas_falsification.cpp"


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def require(text: str, token: str) -> None:
    assert token in text, f"missing Stage 1 contract token: {token}"


def forbid(text: str, token: str) -> None:
    assert token not in text, f"forbidden Stage 1 dependency/behavior: {token}"


def test_types_encode_normative_stage1_contracts() -> None:
    text = read(TYPES)
    for token in (
        "kStepsPerBar = 16",
        "kMaxPhraseBars = 4",
        "struct PhraseRhythmIdentity",
        "immutableAnchors",
        "canonicalAnchors",
        "enum class GateClass",
        "enum class EventImportance",
        "struct ProtectedSpace",
        "enum class RelationshipScope",
        "enum class RealizationStatus",
        "enum class TransformationIntent",
        "struct DensityContract",
        "struct TimingEligibility",
        "RhythmIdentity",
        "P1Variation",
        "P2Variation",
        "P3Transformation",
        "PhysicalBinding",
    ):
        require(text, token)
    forbid(text, "meterNumerator")
    forbid(text, "meterDenominator")


def test_stage1_is_bounded_and_not_runtime_coupled() -> None:
    combined = "\n".join(read(path) for path in (TYPES, CATALOG_H, CATALOG_CPP))
    for token in (
        '"scenes.h"',
        '"miniacid_engine.h"',
        '"genre_manager.h"',
        "SynthPattern",
        "DrumPatternSet",
        "SceneManager",
        "std::vector",
        "std::string",
        "std::map",
        "std::unordered_map",
        "std::random",
        "malloc(",
        "calloc(",
        "realloc(",
        "rand(",
        "new ",
    ):
        forbid(combined, token)


def test_existing_core_ci_owns_stage1_tests() -> None:
    assert not DEDICATED_WORKFLOW.exists(), "temporary Stage 1 workflow must not survive"
    workflow = read(CORE_WORKFLOW)
    require(workflow, "bash tests/run_rhythm_stage1_tests.sh")
    require(workflow, "cardputer-adv-build")
    require(workflow, "cardputer-adv-seqtrak-midi-only-build")


def test_atlas_falsification_uses_hash_gated_v26_inputs() -> None:
    compiler = read(ATLAS_COMPILER)
    require(compiler, "schema_version')!='2.6.0'")
    require(compiler, "EXPECTED_SHA256='5b155937b8d05f0f0f9f1a02f10d9afe76a917d6035897695cce739eb8d6b1fd'")
    test = read(ATLAS_TEST)
    for token in (
        "PAT_ED_ACID_ROLLING_P1",
        "PAT_ED_UKG_CLASSIC_2STEP_P1",
        "PAT_ED_DUB_DEEP_CHORD_P1",
        "PhraseRhythmIdentity",
        "LaneRelationship",
    ):
        require(test, token)


if __name__ == "__main__":
    test_types_encode_normative_stage1_contracts()
    test_stage1_is_bounded_and_not_runtime_coupled()
    test_existing_core_ci_owns_stage1_tests()
    test_atlas_falsification_uses_hash_gated_v26_inputs()
