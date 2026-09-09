#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SONG = (ROOT / "src/ui/pages/song_page.cpp").read_text(encoding="utf-8")
PHRASE = (ROOT / "src/ui/pages/phrase_page.cpp").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def between(text: str, start: str, end: str) -> str:
    start_index = text.index(start)
    end_index = text.index(end, start_index)
    return text[start_index:end_index]


song_hint = between(
    SONG,
    "void SongPage::drawGeneratorHint(IGfx& gfx)",
    "SongPatternMaterializer::Result SongPage::materializeSongTracks",
)

for misleading in ('"RND"', '"SMART"', '"EVOL"', '"FILL"'):
    require(
        misleading not in song_hint,
        f"Song generator hint still exposes misleading musical mode {misleading}",
    )

require(
    '"GEN ALT:%d/4"' in song_hint,
    "Song generator hint must present the existing selector as a neutral generation alternative",
)

phrase_product = between(
    PHRASE,
    "void PhrasePage::drawProductView(IGfx& gfx)",
    "bool PhrasePage::handleProductEvent(UIEvent& ui_event)",
)

require(
    '"DEPTH"' not in phrase_product,
    "Public PHRASE must not present P1/P2/P3 realization policy as musical DEPTH",
)
require(
    '"LEVEL"' in phrase_product,
    "Public PHRASE must label P1/P2/P3 as a neutral realization LEVEL",
)
require(
    '"LAST GEN: %s"' in phrase_product,
    "Public PHRASE must identify the prior generator outcome explicitly instead of LAST G",
)

print("Hybrid Song orchestration UX source regressions: PASS")
