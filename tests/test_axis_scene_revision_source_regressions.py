#!/usr/bin/env python3
"""Source-level gates for FEEL/TEXTURE/GENRE Scene revision ownership."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FEEL = (ROOT / "src/ui/pages/feel_page.cpp").read_text(encoding="utf-8")
TEXTURE = (ROOT / "src/ui/pages/texture_page.cpp").read_text(encoding="utf-8")
GENRE = (ROOT / "src/ui/pages/genre_page.cpp").read_text(encoding="utf-8")
ENGINE = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")
REVISION = (ROOT / "src/state/scene_revision.h").read_text(encoding="utf-8")


def function_body(text: str, signature: str, next_signature: str) -> str:
    start = text.find(signature)
    if start < 0:
        raise AssertionError(f"missing function: {signature}")
    end = text.find(next_signature, start + len(signature))
    if end < 0:
        raise AssertionError(f"missing boundary after: {signature}")
    return text[start:end]


def require(text: str, needle: str, message: str) -> None:
    if needle not in text:
        raise AssertionError(message)


def require_mark_count(text: str, expected: int, owner: str) -> None:
    count = text.count("GroovePuterState::markSceneMutated();")
    if count != expected:
        raise AssertionError(
            f"{owner} must contain {expected} Scene mutation mark(s), found {count}"
        )


feel_adjust = function_body(
    FEEL, "void FeelPage::adjustFocused", "void FeelPage::applyPreset"
)
feel_preset = function_body(
    FEEL, "void FeelPage::applyPreset", "void FeelPage::draw"
)
require(feel_adjust, "bool changed = false;", "FEEL adjustment needs change detection")
require(feel_adjust, "if (changed)", "FEEL adjustment must mark only real changes")
require_mark_count(feel_adjust, 1, "FEEL adjustment")
require(feel_preset, "const bool changed =", "FEEL preset needs change detection")
require(feel_preset, "if (changed)", "FEEL preset must skip no-op revisions")
require_mark_count(feel_preset, 1, "FEEL preset")

texture_browse = function_body(
    TEXTURE, "void TexturePage::shiftTexture", "void TexturePage::toggleFlavorLink"
)
texture_link = function_body(
    TEXTURE, "void TexturePage::toggleFlavorLink", "void TexturePage::applyTexture"
)
texture_apply = function_body(
    TEXTURE, "void TexturePage::applyTexture", "std::array<uint8_t, 7> TexturePage::macroView"
)
require_mark_count(texture_browse, 0, "TEXTURE selectors")
require_mark_count(texture_link, 1, "TEXTURE flavor link")
require(texture_apply, "bool changed = false;", "TEXTURE apply needs change detection")
require(texture_apply, "if (changed)", "TEXTURE apply must skip no-op revisions")
require_mark_count(texture_apply, 1, "TEXTURE apply")

for signature, next_signature, owner in (
    ("void GenrePage::shiftGenre", "void GenrePage::cycleRecipeSelection", "Genre browse"),
    ("void GenrePage::cycleRecipeSelection", "void GenrePage::adjustMorph", "Recipe browse"),
    ("void GenrePage::adjustMorph", "void GenrePage::cycleApplyMode", "Morph browse"),
):
    require_mark_count(function_body(GENRE, signature, next_signature), 0, owner)

genre_mode = function_body(
    GENRE, "void GenrePage::cycleApplyMode", "void GenrePage::applyCurrent"
)
genre_apply = function_body(
    GENRE, "void GenrePage::applyCurrent", "void GenrePage::updateFromEngine"
)
require_mark_count(genre_mode, 1, "Genre apply-mode setting")
require(
    genre_apply,
    "bool changed = doRegenerate ||",
    "Genre Apply must treat materialization as one persistent mutation",
)
require(
    genre_apply,
    "if (changed) GroovePuterState::markSceneMutated();",
    "Genre Apply must mark only after the guarded transaction",
)
require_mark_count(genre_apply, 1, "Genre Apply")

require(
    ENGINE,
    "bool MiniAcid::saveSceneToStorage()",
    "Scene save entry point must remain available",
)
require(
    ENGINE,
    "bool MiniAcid::autoSaveSceneRecovery()",
    "Recovery autosave entry point must remain available",
)
require(
    REVISION,
    "void markSaveSucceeded()",
    "Scene revision service must expose successful Save semantics",
)
require(
    REVISION,
    "void markLoadSucceeded()",
    "Scene revision service must expose successful Load semantics",
)
require(
    REVISION,
    "restoreSceneRevision",
    "Scene revision service must support failed-transaction rollback",
)

print("Axis Scene revision source regressions: PASS")
