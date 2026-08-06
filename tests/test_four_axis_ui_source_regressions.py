#!/usr/bin/env python3
"""Source-level ownership gates for the four-axis GENERATE UI.

The test intentionally checks semantic boundaries rather than renderer details.
No axis page may silently mutate another axis or reintroduce duplicate addresses.
"""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

GENRE = (ROOT / "src/ui/pages/genre_page.cpp").read_text(encoding="utf-8")
FEEL = (ROOT / "src/ui/pages/feel_page.cpp").read_text(encoding="utf-8")
GENERATION = (ROOT / "src/ui/pages/generation_page.cpp").read_text(encoding="utf-8")
TEXTURE = (ROOT / "src/ui/pages/texture_page.cpp").read_text(encoding="utf-8")
WORKFLOW = (ROOT / "src/ui/workflow_mode.h").read_text(encoding="utf-8")
SESSION = (ROOT / "src/state/ui_session_state.h").read_text(encoding="utf-8")
HELP = (ROOT / "src/ui/global_help_content.h").read_text(encoding="utf-8")
PALETTE = (ROOT / "src/ui/axis_page_palette.h").read_text(encoding="utf-8")


PAGE_DIR = ROOT / "src/ui/pages"
CANONICAL_AXIS_FILES = (
    "genre_page.h", "genre_page.cpp",
    "feel_page.h", "feel_page.cpp",
    "generation_page.h", "generation_page.cpp",
    "texture_page.h", "texture_page.cpp",
)
LEGACY_AXIS_FILES = (
    "settings_page.h", "settings_page.cpp",
    "mode_page.h", "mode_page.cpp",
    "feel_texture_page.h", "feel_texture_page.cpp",
)
for filename in CANONICAL_AXIS_FILES:
    if not (PAGE_DIR / filename).is_file():
        raise AssertionError(f"canonical axis source missing: {filename}")
for filename in LEGACY_AXIS_FILES:
    if (PAGE_DIR / filename).exists():
        raise AssertionError(f"legacy axis source must not exist: {filename}")


def require(text: str, needle: str, message: str) -> None:
    if needle not in text:
        raise AssertionError(message)


def forbid(text: str, needles: tuple[str, ...], owner: str) -> None:
    for needle in needles:
        if needle in text:
            raise AssertionError(f"{owner} illegally contains cross-axis token: {needle}")


# GENRE: corridor/vocabulary and explicit materialization policy only.
for needle in (
    '"GENRE 1/4"',
    '"CORRIDOR / VOCABULARY"',
    "setGenerativeMode",
    "setRecipe",
    "regeneratePatternsWithGenre",
    '"PROFILE ONLY"',
    '"MATERIALIZE"',
):
    require(GENRE, needle, f"GENRE contract missing: {needle}")
forbid(
    GENRE,
    (
        "setTextureMode",
        "applyTexture(",
        "applyGenreTimbre",
        "toggleGrooveboxMode",
        "swingPct",
        "microTimingAmount",
        "velocityRange",
        "PhraseGenerator::",
    ),
    "GENRE",
)

# FEEL: live timing/velocity point only. Selecting a preset must not dirty Scene.
for needle in (
    '"FEEL 2/4"',
    '"TIMING / VELOCITY"',
    "scene.feel.swingPct",
    "microTimingAmount",
    "velocityRange",
    "if (focus_ == FocusRow::Preset)",
    "preset_index_ = wrapIndex",
):
    require(FEEL, needle, f"FEEL contract missing: {needle}")
forbid(
    FEEL,
    (
        "ghostNoteProbability",
        "minNotes",
        "maxNotes",
        "scaleRoot",
        "scaleQuantize",
        "measureSDPerformance",
        "setTextureMode",
        "applyTexture(",
        "PhraseGenerator::",
    ),
    "FEEL",
)

# GENERATION: phrase form/materialization only.
for needle in (
    '"GEN 3/4"',
    '"FORM / DEVELOPMENT"',
    "PhraseGenerator::PhraseRequest",
    "PhraseGenerator::generateBarsToSong",
    "PhraseGenerator::generateToSong",
):
    require(GENERATION, needle, f"GENERATION contract missing: {needle}")
forbid(
    GENERATION,
    (
        "setTextureMode",
        "applyTexture(",
        "applySoundMacros",
        "toggleMacros",
        "grooveFlavor",
        "shiftFlavor",
        "microTimingAmount",
        "velocityRange",
        "swingPct",
        "randomize303Pattern",
    ),
    "GENERATION",
)


require(GENERATION, "request.bars = kMaterializeBars;",
        "GENERATION request must use the fixed single-bar scope")
require(GENERATION, "Phrase length owned by PHRASE CORE",
        "GENERATION must disclose the cross-workflow length owner")
forbid(
    GENERATION,
    (
        "phrase_bars_",
        "shiftPhraseLength",
        "kLengths[4]",
        '"LENGTH"',
        "L/R:LENGTH",
        "1, 2, 4, 8",
    ),
    "GENERATION phrase-length ownership",
)

length_owner_tokens = ("capture_length_", "cycleLength(")
unexpected_length_owners = []
for candidate in PAGE_DIR.glob("*_page.*"):
    page_source = candidate.read_text(encoding="utf-8")
    if any(token in page_source for token in length_owner_tokens):
        if candidate.name not in {"phrase_page.h", "phrase_page.cpp"}:
            unexpected_length_owners.append(candidate.name)
if unexpected_length_owners:
    raise AssertionError(
        "selected phrase length has duplicate UI owners: "
        + ", ".join(sorted(unexpected_length_owners))
    )

# TEXTURE: sound surface and seven read-only macro projection only.
for needle in (
    '"TEXTURE 4/4"',
    '"SOUND SURFACE"',
    "setTextureMode",
    "applyTexture(mini_acid_)",
    "std::array<uint8_t, 7>",
    '"MACRO VIEW 0..127 (READ ONLY)"',
    '"FLAVOR LINK"',
):
    require(TEXTURE, needle, f"TEXTURE contract missing: {needle}")
forbid(
    TEXTURE,
    (
        "PhraseGenerator::",
        "regeneratePatternsWithGenre",
        "gridSteps",
        "timebase",
        "patternBars",
        "swingPct",
        "microTimingAmount",
        "velocityRange",
        "minNotes",
        "maxNotes",
    ),
    "TEXTURE",
)

# One fixed address per axis, in causal order.
require(WORKFLOW, "case WorkflowMode::Generate: return 4;",
        "GENERATE must expose four pages")
require(
    WORKFLOW,
    "kGenre, kFeel, kGeneration, kTexture",
    "GENERATE order must be GENRE -> FEEL -> GENERATION -> TEXTURE",
)
for title in ('return "GENRE";', 'return "FEEL";', 'return "GENERATION";', 'return "TEXTURE";'):
    require(WORKFLOW, title, f"Workflow page title missing: {title}")
require(WORKFLOW, "case WorkflowMode::Settings: return 1;",
        "FEEL must no longer live in SETTINGS")


for legacy in ("kGenerator", "kMode", "kFeelTexture"):
    if legacy in WORKFLOW or legacy in SESSION:
        raise AssertionError(f"legacy page address remains in source: {legacy}")

# Persisted workflow mapping must mirror runtime navigation.
require(
    SESSION,
    "page == SessionPages::kFeel",
    "persisted FEEL page must belong to GENERATE",
)
require(SESSION, "case SessionWorkflow::Generate: return 4;",
        "persisted GENERATE count must be four")
require(SESSION, "case SessionWorkflow::Settings: return 1;",
        "persisted SETTINGS count must be one")

# Alt+H and visual themes are part of the page contract.
for title in ("GENRE 1/4", "FEEL 2/4", "GENERATION 3/4", "TEXTURE 4/4"):
    require(HELP, title, f"Alt+H section missing: {title}")
for semantic_guard in (
    "No texture or feel changes",
    "No notes, roles or sound changes",
    "No texture or microtiming changes",
    "No note or rhythm changes",
):
    require(HELP, semantic_guard, f"Alt+H semantic guard missing: {semantic_guard}")
for style in ("VisualStyle::MINIMAL", "VisualStyle::RETRO_CLASSIC", "VisualStyle::AMBER"):
    require(PALETTE, style, f"Four-axis palette missing style: {style}")

print("Four-axis UI source regressions: PASS")
