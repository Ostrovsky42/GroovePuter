#!/usr/bin/env python3
"""Source-level ownership gates for the current GENERATE workflow.

The historical GENRE/FEEL/GENERATION/TEXTURE model is now two user-facing
pages: GENRE and FEEL. Persisted GENERATION/TEXTURE page ids remain decode-only
compatibility aliases and must resolve to FEEL instead of reviving old pages.
"""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PAGE_DIR = ROOT / "src/ui/pages"

GENRE = (PAGE_DIR / "genre_page.cpp").read_text(encoding="utf-8")
FEEL = (PAGE_DIR / "feel_page.cpp").read_text(encoding="utf-8")
WORKFLOW = (ROOT / "src/ui/workflow_mode.h").read_text(encoding="utf-8")
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
):
    if not (PAGE_DIR / filename).is_file():
        raise AssertionError(f"canonical GENERATE source missing: {filename}")

for filename in (
    "generation_page.h", "generation_page.cpp",
    "texture_page.h", "texture_page.cpp",
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
        "setTextureMode", "applyTexture(", "applyGenreTimbre",
        "microTimingAmount", "velocityRange", "PhraseGenerator::",
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
        "scaleQuantize", "setTextureMode", "applyTexture(",
        "PhraseGenerator::",
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
        "GENERATE must expose exactly GENRE and FEEL")
require(WORKFLOW, "kGenre, kFeel,",
        "GENERATE order must be GENRE -> FEEL")
require(WORKFLOW, "constexpr int kTexture = 8;",
        "historical TEXTURE page id 8 must remain reserved")
require(WORKFLOW, "constexpr int kGeneration = 11;",
        "historical GENERATION page id 11 must remain reserved")
require(WORKFLOW, "if (page == kTexture || page == kGeneration) return kFeel;",
        "legacy GENERATION/TEXTURE page ids must normalize to FEEL")
for title in ('return "GENRE";', 'return "FEEL";'):
    require(WORKFLOW, title, f"Workflow page title missing: {title}")
generate_list = WORKFLOW.split(
    "static constexpr int kGeneratePages[]", 1
)[1].split("};", 1)[0]
forbid(generate_list, ("kTexture", "kGeneration"),
       "normal GENERATE navigation")

for title in ("GENRE 1/2", "FEEL 2/2"):
    require(HELP, title, f"Alt+H section missing: {title}")
forbid(
    HELP,
    ("GENERATION 3/3", "GEN 3/3", "TEXTURE 4/4", "LIVE SOUND SURFACE"),
    "Alt+H",
)
for semantic_guard in (
    "No texture or feel changes",
    "No notes, roles or sound changes",
):
    require(HELP, semantic_guard, f"Alt+H semantic guard missing: {semantic_guard}")
for style in (
    "VisualStyle::MINIMAL",
    "VisualStyle::RETRO_CLASSIC",
    "VisualStyle::AMBER",
):
    require(PALETTE, style, f"GENERATE palette missing style: {style}")

print("Two-page GENERATE UI source regressions: PASS")
