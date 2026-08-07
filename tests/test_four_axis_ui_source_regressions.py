#!/usr/bin/env python3
"""Source-level ownership gates for the GENERATE workflow.

The historical four-axis layout is now intentionally three user-facing pages:
GENRE, FEEL and GENERATION. Persisted TEXTURE addresses remain compatibility
aliases only and must never return to normal navigation.
"""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PAGE_DIR = ROOT / "src/ui/pages"

GENRE = (PAGE_DIR / "genre_page.cpp").read_text(encoding="utf-8")
FEEL = (PAGE_DIR / "feel_page.cpp").read_text(encoding="utf-8")
GENERATION = (PAGE_DIR / "generation_page.cpp").read_text(encoding="utf-8")
WORKFLOW = (ROOT / "src/ui/workflow_mode.h").read_text(encoding="utf-8")
SESSION = (ROOT / "src/state/ui_session_state.h").read_text(encoding="utf-8")
HELP = (ROOT / "src/ui/global_help_content.h").read_text(encoding="utf-8")
PALETTE = (ROOT / "src/ui/axis_page_palette.h").read_text(encoding="utf-8")


def require(text: str, needle: str, message: str) -> None:
    if needle not in text:
        raise AssertionError(message)


def forbid(text: str, needles: tuple[str, ...], owner: str) -> None:
    for needle in needles:
        if needle in text:
            raise AssertionError(f"{owner} illegally contains token: {needle}")


for filename in (
    "genre_page.h", "genre_page.cpp",
    "feel_page.h", "feel_page.cpp",
    "generation_page.h", "generation_page.cpp",
):
    if not (PAGE_DIR / filename).is_file():
        raise AssertionError(f"canonical GENERATE source missing: {filename}")

for filename in (
    "texture_page.h", "texture_page.cpp",
    "settings_page.h", "settings_page.cpp",
    "mode_page.h", "mode_page.cpp",
    "feel_texture_page.h", "feel_texture_page.cpp",
):
    if (PAGE_DIR / filename).exists():
        raise AssertionError(f"removed or legacy axis source must not exist: {filename}")

for needle in (
    '"GENRE 1/3"',
    '"CORRIDOR / VOCABULARY"',
    "settings.generativeMode =",
    "settings.recipe =",
    "GenreCatalog::grooveboxModeForRecipe",
    "regeneratePatternsWithGenre",
    '"PROFILE ONLY"',
    '"MATERIALIZE"',
):
    require(GENRE, needle, f"GENRE contract missing: {needle}")
forbid(
    GENRE,
    (
        ".genreManager()", "setTextureMode", "applyTexture(",
        "applyGenreTimbre", "toggleGrooveboxMode", "swingPct",
        "microTimingAmount", "velocityRange", "PhraseGenerator::",
    ),
    "GENRE",
)

for needle in (
    '"FEEL 2/3"',
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
        "ghostNoteProbability", "minNotes", "maxNotes", "scaleRoot",
        "scaleQuantize", "measureSDPerformance", "setTextureMode",
        "applyTexture(", "PhraseGenerator::",
    ),
    "FEEL",
)

for needle in (
    '"GEN 3/3"',
    '"WRITE ONE SONG BAR"',
    "PhraseGenerator::PhraseRequest",
    "PhraseGenerator::generateBarsToSong",
    "PhraseGenerator::generateToSong",
    "request.bars = kMaterializeBars;",
    '"CURRENT EMPTY SONG ROW"',
):
    require(GENERATION, needle, f"GENERATION contract missing: {needle}")
forbid(
    GENERATION,
    (
        "setTextureMode", "applyTexture(", "applySoundMacros",
        "toggleMacros", "grooveFlavor", "shiftFlavor",
        "microTimingAmount", "velocityRange", "swingPct",
        "randomize303Pattern", "phrase_bars_", "shiftPhraseLength",
        "kLengths[4]", '"LENGTH"', "L/R:LENGTH", "1, 2, 4, 8",
    ),
    "GENERATION",
)

length_owner_tokens = ("capture_length_", "cycleLength(")
unexpected_length_owners = []
for candidate in PAGE_DIR.glob("*_page.*"):
    source = candidate.read_text(encoding="utf-8")
    if any(token in source for token in length_owner_tokens):
        if candidate.name not in {"phrase_page.h", "phrase_page.cpp"}:
            unexpected_length_owners.append(candidate.name)
if unexpected_length_owners:
    raise AssertionError(
        "selected phrase length has duplicate UI owners: "
        + ", ".join(sorted(unexpected_length_owners))
    )

require(WORKFLOW, "case WorkflowMode::Generate: return 3;",
        "GENERATE must expose three pages")
require(WORKFLOW, "kGenre, kFeel, kGeneration,",
        "GENERATE order must be GENRE -> FEEL -> GENERATION")
require(WORKFLOW, "constexpr int kTexture = 8;",
        "historical page id 8 must remain reserved")
require(WORKFLOW, "case kTexture:",
        "historical page id 8 must be handled")
require(WORKFLOW, "case Workspace::Texture:",
        "historical workspace value must be handled")
require(WORKFLOW, "if (page == kTexture) page = kFeel;",
        "legacy page index must normalize to FEEL")
for title in ('return "GENRE";', 'return "FEEL";', 'return "GENERATION";'):
    require(WORKFLOW, title, f"Workflow page title missing: {title}")
forbid(
    WORKFLOW.split("static constexpr int kGeneratePages[]", 1)[1]
            .split("};", 1)[0],
    ("kTexture",),
    "normal GENERATE navigation",
)

require(SESSION, "case SessionWorkflow::Generate: return 3;",
        "persisted GENERATE count must be three")
require(SESSION, "SessionPages::kGenre,",
        "persisted GENERATE list must contain GENRE")
require(SESSION, "SessionPages::kFeel,",
        "persisted GENERATE list must contain FEEL")
require(SESSION, "SessionPages::kGeneration,",
        "persisted GENERATE list must contain GENERATION")
require(SESSION, "normalizeLegacyUiPage",
        "persisted page id 8 must have an explicit normalizer")
require(SESSION, "SessionPages::kTexture ? SessionPages::kFeel",
        "persisted page id 8 must resolve to FEEL")
session_generate_list = SESSION.split(
    "static constexpr int kGeneratePages[]", 1
)[1].split("};", 1)[0]
forbid(session_generate_list, ("SessionPages::kTexture",),
       "persisted normal GENERATE navigation")

for title in ("GENRE 1/3", "FEEL 2/3", "GENERATION 3/3"):
    require(HELP, title, f"Alt+H section missing: {title}")
forbid(HELP, ("TEXTURE 4/4", "LIVE SOUND SURFACE"), "Alt+H")
for semantic_guard in (
    "No texture or feel changes",
    "No notes, roles or sound changes",
    "No texture or microtiming changes",
):
    require(HELP, semantic_guard, f"Alt+H semantic guard missing: {semantic_guard}")
for style in ("VisualStyle::MINIMAL", "VisualStyle::RETRO_CLASSIC", "VisualStyle::AMBER"):
    require(PALETTE, style, f"GENERATE palette missing style: {style}")

print("Three-page GENERATE UI source regressions: PASS")
