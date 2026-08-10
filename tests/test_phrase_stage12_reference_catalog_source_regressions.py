from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / "src/generation/rhythm/reference_phrase_vocabulary.h").read_text(encoding="utf-8")
SOURCE = (ROOT / "src/generation/rhythm/reference_phrase_vocabulary.cpp").read_text(encoding="utf-8")
BASE = (ROOT / "src/generation/rhythm/reference_vocabulary.cpp").read_text(encoding="utf-8")
BRIDGE = (ROOT / "src/generation/migration/strong_rhythm_live_bridge.cpp").read_text(encoding="utf-8")
MIGRATION = (ROOT / "src/generation/migration/strong_rhythm_migration.cpp").read_text(encoding="utf-8")


def require(text: str, needle: str, message: str) -> None:
    if needle not in text:
        raise AssertionError(message)


require(HEADER, "phraseEvolutionCatalog", "Stage 12 candidate catalog API disappeared")
require(HEADER, "phraseEvolutionEnabled", "Stage 12 capability query disappeared")

for needle in (
    "phraseBarsBit(1) | phraseBarsBit(2) | phraseBarsBit(4)",
    "BarFunction::Reduction",
    "BarFunction::Break",
    "AllowReduction",
    "AllowTurnaround",
    "AllowBreak",
    "case 413",
    "case 417",
    "case 420",
    "case 712",
):
    require(SOURCE, needle, f"Stage 12 candidate catalog lost contract: {needle}")

# The accepted one-bar production path remains bit-for-bit owned by the original
# ReferenceVocabulary until the physical ESP32-S3 gate is recorded.
require(
    BASE,
    "value.allowedPhraseBars = phraseBarsBit(1);",
    "production ReferenceVocabulary was widened before hardware gate",
)
require(
    BASE,
    "value.trajectories = &kStatementRef;",
    "production ReferenceVocabulary trajectory ownership changed",
)
require(
    MIGRATION,
    "request.phraseBars = 1;",
    "production strong migration escaped the one-bar hardware guard",
)
for production_source in (BRIDGE, MIGRATION):
    if "phraseEvolutionCatalog" in production_source or "evolveMultiBarPhrase" in production_source:
        raise AssertionError("Stage 12 candidate became production-reachable before hardware gate")

# The overlay is fixed-capacity and must not grow a second Scene/Song owner or
# dynamic allocation path.
for forbidden in ("Scene", "Song", "PhraseCore", "new ", "malloc(", "rand("):
    if forbidden in SOURCE:
        raise AssertionError(f"Stage 12 reference catalog leaked forbidden owner/path: {forbidden}")

print("Generation Stage 12 reference catalog source regressions: OK")
