#!/usr/bin/env python3
"""Source-level ownership gates for the GENERATE workflow.

GENERATE has exactly two user-facing pages: GENRE and FEEL. Historical TEXTURE
(page 8) and GENERATION (page 11) addresses are compatibility aliases only and
must resolve to FEEL without returning to normal navigation.
"""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PAGE_DIR = ROOT / "src/ui/pages"

GENRE = (PAGE_DIR / "genre_page.cpp").read_text(encoding="utf-8")
FEEL = (PAGE_DIR / "feel_page.cpp").read_text(encoding="utf-8")
WORKFLOW = (ROOT / "src/ui/workflow_mode.h").read_text(encoding="utf-8")
SESSION = (ROOT / "src/state/ui_session_state.h").read_text(encoding="utf-8")
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
):
    if not (PAGE_DIR / filename).is_file():
        raise AssertionError(f"canonical GENERATE source missing: {filename}")

for filename in (
    "texture_page.h", "texture_page.cpp",
    "generation_page.h", "generation_page.cpp",
):
    if (PAGE_DIR / filename).exists():
        raise AssertionError(f"retired GENERATE source must not exist: {filename}")

for needle in (
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
        ".genreView()", "setTextureMode", "applyTexture(",
        "applyGenreTimbre", "toggleGrooveboxMode", "PhraseGenerator::",
    ),
    "GENRE",
)

for needle in (
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

require(WORKFLOW, "case WorkflowMode::Generate: return 2;",
        "GENERATE must expose exactly two pages")
require(WORKFLOW, "kGenre, kFeel,",
        "GENERATE order must be GENRE -> FEEL")
require(WORKFLOW, "constexpr int kTexture = 8;",
        "historical page id 8 must remain reserved")
require(WORKFLOW, "constexpr int kGeneration = 11;",
        "historical page id 11 must remain reserved")
require(WORKFLOW, "page == kTexture || page == kGeneration",
        "legacy page ids 8 and 11 must normalize together")
for title in ('return "GENRE";', 'return "FEEL";'):
    require(WORKFLOW, title, f"Workflow page title missing: {title}")
normal_generate = WORKFLOW.split(
    "static constexpr int kGeneratePages[]", 1
)[1].split("};", 1)[0]
forbid(normal_generate, ("kTexture", "kGeneration"),
       "normal GENERATE navigation")

require(SESSION, "case SessionWorkflow::Generate: return 2;",
        "persisted GENERATE count must be two")
require(SESSION, "SessionPages::kGenre,",
        "persisted GENERATE list must contain GENRE")
require(SESSION, "SessionPages::kFeel,",
        "persisted GENERATE list must contain FEEL")
require(SESSION, "page == SessionPages::kTexture ||",
        "persisted page id 8 must be normalized")
require(SESSION, "page == SessionPages::kGeneration",
        "persisted page id 11 must be normalized")
session_generate_list = SESSION.split(
    "static constexpr int kGeneratePages[]", 1
)[1].split("};", 1)[0]
forbid(session_generate_list,
       ("SessionPages::kTexture", "SessionPages::kGeneration"),
       "persisted normal GENERATE navigation")

for style in ("VisualStyle::MINIMAL", "VisualStyle::RETRO_CLASSIC", "VisualStyle::AMBER"):
    require(PALETTE, style, f"GENERATE palette missing style: {style}")

print("Two-page GENERATE UI source regressions: PASS")
