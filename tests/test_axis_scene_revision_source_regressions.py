#!/usr/bin/env python3
"""Source gates for persistent FEEL/GENRE Scene revision ownership."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FEEL = (ROOT / "src/ui/pages/feel_page.cpp").read_text(encoding="utf-8")
GENRE = (ROOT / "src/ui/pages/genre_page.cpp").read_text(encoding="utf-8")
REVISION = (ROOT / "src/state/scene_revision.h").read_text(encoding="utf-8")


def body(text: str, start_token: str, end_token: str) -> str:
    start = text.find(start_token)
    if start < 0:
        raise AssertionError(f"missing function: {start_token}")
    end = text.find(end_token, start + len(start_token))
    if end < 0:
        raise AssertionError(f"missing function boundary after: {start_token}")
    return text[start:end]


def require(text: str, token: str, message: str) -> None:
    if token not in text:
        raise AssertionError(message)


def require_marks(text: str, expected: int, owner: str) -> None:
    actual = text.count("GroovePuterState::markSceneMutated();")
    if actual != expected:
        raise AssertionError(
            f"{owner} must contain {expected} mutation mark(s), found {actual}"
        )


feel_adjust = body(FEEL, "void FeelPage::adjustFocused", "void FeelPage::applyPreset")
feel_preset = body(FEEL, "void FeelPage::applyPreset", "void FeelPage::draw")
require(feel_adjust, "bool changed = false;", "FEEL adjustment needs change detection")
require(feel_adjust, "if (changed)", "FEEL adjustment must skip clamped no-ops")
require_marks(feel_adjust, 1, "FEEL adjustment")
require(feel_preset, "const bool changed =", "FEEL preset needs change detection")
require(feel_preset, "if (changed)", "FEEL preset must skip repeated no-op apply")
require_marks(feel_preset, 1, "FEEL preset")

for start, end, owner in (
    ("void GenrePage::shiftGenre", "void GenrePage::cycleRecipeSelection", "Genre browse"),
    ("void GenrePage::cycleRecipeSelection", "void GenrePage::adjustMorph", "Recipe browse"),
    ("void GenrePage::adjustMorph", "void GenrePage::cycleApplyMode", "Morph browse"),
):
    require_marks(body(GENRE, start, end), 0, owner)

genre_mode = body(GENRE, "void GenrePage::cycleApplyMode", "void GenrePage::applyCurrent")
genre_apply = body(GENRE, "void GenrePage::applyCurrent", "void GenrePage::updateFromEngine")
require_marks(genre_mode, 1, "Genre apply-mode setting")
require(
    genre_apply,
    "const bool changed = doRegenerate ||",
    "Genre materialization must count as one logical mutation",
)
require(
    genre_apply,
    "if (changed) GroovePuterState::markSceneMutated();",
    "Genre Apply must mark after the guarded transaction",
)
require_marks(genre_apply, 1, "Genre Apply")

require(
    REVISION,
    "#ifndef GROOVEPUTER_SRC_STATE_SCENE_REVISION_H_",
    "Scene revision header needs an explicit Arduino alias-path include guard",
)
require(
    REVISION,
    "#define GROOVEPUTER_SRC_STATE_SCENE_REVISION_H_",
    "Scene revision include guard must be defined",
)
require(REVISION, "void markSaveSucceeded()", "revision service must expose Save success")
require(REVISION, "void markLoadSucceeded()", "revision service must expose Load success")
require(REVISION, "restoreSceneRevision", "revision service must support rollback")

if (ROOT / "src/ui/pages/texture_page.cpp").exists():
    raise AssertionError("removed TEXTURE page must not be restored by stabilization")

print("FEEL/GENRE Scene revision source regressions: PASS")
