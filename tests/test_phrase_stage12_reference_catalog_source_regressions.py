from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = (
    ROOT / "src/generation/rhythm/reference_phrase_vocabulary.h"
).read_text(encoding="utf-8")
SOURCE = (
    ROOT / "src/generation/rhythm/reference_phrase_vocabulary.cpp"
).read_text(encoding="utf-8")
BASE = (
    ROOT / "src/generation/rhythm/reference_vocabulary.cpp"
).read_text(encoding="utf-8")
BRIDGE = (
    ROOT / "src/generation/migration/strong_rhythm_live_bridge.cpp"
).read_text(encoding="utf-8")
MIGRATION = (
    ROOT / "src/generation/migration/strong_rhythm_migration.cpp"
).read_text(encoding="utf-8")


def require(text: str, needle: str, message: str) -> None:
    if needle not in text:
        raise AssertionError(message)


require(
    HEADER,
    "phraseEvolutionCatalog",
    "Stage 12 candidate catalog API disappeared",
)
require(
    HEADER,
    "phraseEvolutionEnabled",
    "Stage 12 capability query disappeared",
)

for needle in (
    "phraseBarsBit(1) | phraseBarsBit(2) | phraseBarsBit(4)",
    "BarFunction::Reduction",
    "BarFunction::Break",
    "AllowReduction",
    "AllowTurnaround",
    "AllowBreak",
    "kSubtractivePhraseTrajectoryRefs",
    "kNonSubtractivePhraseTrajectoryRefs",
    "stage12SubtractiveEnabledId",
    "id != 416",
    "case 413",
    "case 416",
    "case 417",
    "case 420",
    "case 712",
):
    require(
        SOURCE,
        needle,
        f"Stage 12 candidate catalog lost contract: {needle}",
    )

require(
    SOURCE,
    "archetypes[index].trajectories = kNonSubtractivePhraseTrajectoryRefs;",
    "halftime_switch non-subtractive trajectory path disappeared",
)
require(
    SOURCE,
    "archetypes[index].mutation = stage12NonSubtractiveMutationPolicy();",
    "halftime_switch non-subtractive mutation policy disappeared",
)

# Normal production remains one-bar. The wider Stage 12 candidate catalog is
# reachable only through the explicit audition/probe path restored for 0.9.1.
require(
    BASE,
    "value.allowedPhraseBars = phraseBarsBit(1);",
    "normal production ReferenceVocabulary widened beyond one bar",
)
require(
    BASE,
    "value.trajectories = &kStatementRef;",
    "normal production ReferenceVocabulary trajectory ownership changed",
)
require(
    MIGRATION,
    "request.phraseBars = 1;",
    "normal strong migration escaped its one-bar contract",
)
if "phraseEvolutionCatalog" in MIGRATION or "evolveMultiBarPhrase" in MIGRATION:
    raise AssertionError(
        "normal strong migration gained Stage 12 multi-bar ownership"
    )

require(
    BRIDGE,
    "void runSubtractiveRuntimeProbe",
    "Stage 12 physical runtime probe disappeared",
)
require(
    BRIDGE,
    "PhraseAuditionResult regeneratePhraseAuditionWithProbe",
    "explicit Stage 12 audition owner disappeared",
)
require(
    BRIDGE,
    "ReferenceVocabulary::phraseEvolutionCatalog()",
    "explicit Stage 12 audition/probe lost the candidate catalog",
)
require(
    BRIDGE,
    "evolveMultiBarPhrase(",
    "explicit Stage 12 audition/probe lost multi-bar evolution",
)

probe_start = BRIDGE.index("void runSubtractiveRuntimeProbe")
probe_end = BRIDGE.index("void printProbe", probe_start)
audition_start = BRIDGE.index(
    "PhraseAuditionResult regeneratePhraseAuditionWithProbe"
)

probe_section = BRIDGE[probe_start:probe_end]
audition_section = BRIDGE[audition_start:]
outside_allowed_stage12 = BRIDGE[:probe_start] + BRIDGE[probe_end:audition_start]

for section, name in (
    (probe_section, "runtime probe"),
    (audition_section, "audition owner"),
):
    require(
        section,
        "ReferenceVocabulary::phraseEvolutionCatalog()",
        f"Stage 12 {name} lost candidate-catalog ownership",
    )
    require(
        section,
        "evolveMultiBarPhrase(",
        f"Stage 12 {name} lost multi-bar evolution",
    )

for forbidden in ("phraseEvolutionCatalog", "evolveMultiBarPhrase"):
    if forbidden in outside_allowed_stage12:
        raise AssertionError(
            f"Stage 12 candidate leaked outside explicit audition/probe owners: {forbidden}"
        )

for forbidden in (
    "Scene",
    "Song",
    "PhraseCore",
    "new ",
    "malloc(",
    "rand(",
):
    if forbidden in SOURCE:
        raise AssertionError(
            f"Stage 12 reference catalog leaked forbidden owner/path: {forbidden}"
        )

print("Generation Stage 12 reference catalog source regressions: OK")
