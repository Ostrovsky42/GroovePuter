#!/usr/bin/env python3
"""Source-level ownership gates for the four-axis GENERATE UI.

The test intentionally checks semantic boundaries rather than renderer details.
No axis page may silently mutate another axis or reintroduce duplicate addresses.
"""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

GENRE = (ROOT / "src/ui/pages/genre_page.cpp").read_text(encoding="utf-8")
FEEL = (ROOT / "src/ui/pages/settings_page.cpp").read_text(encoding="utf-8")
GENERATION = (ROOT / "src/ui/pages/mode_page.cpp").read_text(encoding="utf-8")
TEXTURE = (ROOT / "src/ui/pages/feel_texture_page.cpp").read_text(encoding="utf-8")
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
    '"Linear constructive pass / no retry"',
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
    "kGenre, kGenerator, kMode, kFeelTexture",
    "GENERATE order must be GENRE -> FEEL -> GENERATION -> TEXTURE",
)
for title in ('return "GENRE";', 'return "FEEL";', 'return "GENERATION";', 'return "TEXTURE";'):
    require(WORKFLOW, title, f"Workflow page title missing: {title}")
require(WORKFLOW, "case WorkflowMode::Settings: return 1;",
        "FEEL must no longer live in SETTINGS")

# Persisted workflow mapping must mirror runtime navigation.
require(
    SESSION,
    "page == SessionPages::kGenerator",
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
