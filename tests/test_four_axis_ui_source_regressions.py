#!/usr/bin/env python3
"""Source-level ownership gates for the current workflow model.

GENERATION and TEXTURE standalone pages were removed. GENERATE now contains
exactly GENRE and FEEL. Their historical persisted page/workspace IDs remain
compatibility aliases that normalize to FEEL. Legacy standalone synth parameter
page IDs similarly normalize to the owning SYNTH A/B track pages.
"""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PAGE_DIR = ROOT / "src/ui/pages"

GENRE = (PAGE_DIR / "genre_page.cpp").read_text(encoding="utf-8")
FEEL = (PAGE_DIR / "feel_page.cpp").read_text(encoding="utf-8")
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
):
    if not (PAGE_DIR / filename).is_file():
        raise AssertionError(f"canonical GENERATE source missing: {filename}")

for filename in (
    "generation_page.h", "generation_page.cpp",
    "texture_page.h", "texture_page.cpp",
    "settings_page.h", "settings_page.cpp",
    "mode_page.h", "mode_page.cpp",
    "feel_texture_page.h", "feel_texture_page.cpp",
):
    if (PAGE_DIR / filename).exists():
        raise AssertionError(f"removed or legacy axis source must not exist: {filename}")

for needle in (
    '"GENRE 1/2"',
    '"CORRIDOR / VOCABULARY"',
    "GenreCatalog::grooveboxModeForRecipe",
    '"PROFILE ONLY"',
    '"MATERIALIZE"',
):
    require(GENRE, needle, f"GENRE contract missing: {needle}")

# Full generation may now build a complete requested Genre state without
# mutating the active sounding Scene until BAR_START. Accept either the legacy
# direct-settings spelling or the current transactional requested-settings
# spelling, but require both Genre and Recipe ownership to remain on this page.
for field in ("generativeMode", "recipe"):
    if not (
        f"settings.{field} =" in GENRE or
        f"requestedSettings.{field} =" in GENRE
    ):
        raise AssertionError(f"GENRE contract missing state owner: {field}")

if not (
    "regeneratePatternsWithGenre" in GENRE or
    "regenerateWithStrongRhythmMigration" in GENRE or
    "regenerateWithQuantizedCommit" in GENRE
):
    raise AssertionError("GENRE MATERIALIZE has no generation boundary")
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
    '"FEEL 2/2"',
    '"TIMING / VELOCITY"',
    "scene.feel.timingProfile",
    "mini_acid_.applyFeelTimingFromScene_();",
    "scene.feel.swingPct",
    "microTimingAmount",
    "velocityRange",
    "FocusRow::Repeats",
    "scene.feel.patternBars",
    "shiftRepeatBars",
    '"REPEATS"',
    "if (focus_ == FocusRow::Preset)",
    "preset_index_ = wrapIndex",
):
    require(FEEL, needle, f"FEEL contract missing: {needle}")
for repeat_value in ("1, 2, 4, 8", '"CYCLE: repeat 1/2/4/8 bars"'):
    require(FEEL, repeat_value, f"FEEL repeat contract missing: {repeat_value}")
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
        "GENERATE must expose two pages")
require(WORKFLOW, "kGenre, kFeel,",
        "GENERATE order must be GENRE -> FEEL")
require(WORKFLOW, "constexpr int kTexture = 8;",
        "historical TEXTURE page id must remain reserved")
require(WORKFLOW, "constexpr int kGeneration = 11;",
        "historical GENERATION page id must remain reserved")
require(WORKFLOW, "if (page == kTexture || page == kGeneration) return kFeel;",
        "legacy GENERATION/TEXTURE ids must normalize to FEEL")
require(WORKFLOW, "if (page == kSynthAParameters) return kSynthA;",
        "legacy Synth A parameter page must normalize to SYNTH A")
require(WORKFLOW, "if (page == kSynthBParameters) return kSynthB;",
        "legacy Synth B parameter page must normalize to SYNTH B")
for title in ('return "GENRE";', 'return "FEEL";'):
    require(WORKFLOW, title, f"Workflow page title missing: {title}")
generate_list = WORKFLOW.split(
    "static constexpr int kGeneratePages[]", 1
)[1].split("};", 1)[0]
forbid(generate_list, ("kTexture", "kGeneration"),
       "normal GENERATE navigation")
hub_list = WORKFLOW.split(
    "static constexpr int kHubPages[]", 1
)[1].split("};", 1)[0]
forbid(hub_list, ("kSynthAParameters", "kSynthBParameters"),
       "normal HUB navigation")

require(SESSION, "case SessionWorkflow::Generate: return 2;",
        "persisted GENERATE count must be two")
require(SESSION, "case SessionWorkflow::Hub: return 4;",
        "persisted HUB count must be four")
require(SESSION, "SessionPages::kGenre,",
        "persisted GENERATE list must contain GENRE")
require(SESSION, "SessionPages::kFeel,",
        "persisted GENERATE list must contain FEEL")
require(SESSION, "normalizeLegacyUiPage",
        "persisted legacy page ids need an explicit normalizer")
require(SESSION, "page == SessionPages::kGeneration",
        "persisted GENERATION id must normalize")
require(SESSION, "return SessionPages::kFeel;",
        "persisted GENERATION/TEXTURE ids must resolve to FEEL")
require(SESSION, "return SessionPages::kSynthA;",
        "persisted Synth A parameter id must resolve to SYNTH A")
require(SESSION, "return SessionPages::kSynthB;",
        "persisted Synth B parameter id must resolve to SYNTH B")
session_generate_list = SESSION.split(
    "static constexpr int kGeneratePages[]", 1
)[1].split("};", 1)[0]
forbid(session_generate_list,
       ("SessionPages::kTexture", "SessionPages::kGeneration"),
       "persisted normal GENERATE navigation")
session_hub_list = SESSION.split(
    "static constexpr int kHubPages[]", 1
)[1].split("};", 1)[0]
forbid(session_hub_list,
       ("SessionPages::kSynthAParameters", "SessionPages::kSynthBParameters"),
       "persisted normal HUB navigation")

for title in ("GENRE 1/2", "FEEL 2/2"):
    require(HELP, title, f"Alt+H section missing: {title}")
forbid(HELP,
       ("GENERATION 3/3", "GEN 3/3", "TEXTURE 4/4", "LIVE SOUND SURFACE"),
       "Alt+H")
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

print("Two-page GENERATE/UI alias source regressions: PASS")
