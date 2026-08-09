#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STAGE = ROOT / "src" / "generation" / "audition_stage7"
CATALOG = (STAGE / "stage7a_catalog.cpp").read_text(encoding="utf-8")
SESSION = (STAGE / "stage7a_session.cpp").read_text(encoding="utf-8")
CARDPUTER = (STAGE / "stage7a_cardputer.h").read_text(encoding="utf-8")
GENRE_PAGE = (ROOT / "src" / "ui" / "pages" / "genre_page.cpp").read_text(encoding="utf-8")
MAIN_SKETCH = (ROOT / "GroovePuter.ino").read_text(encoding="utf-8")
MANIFEST = (ROOT / "docs" / "architecture" / "stage7a" / "ATLAS_PASS2_CURATION_MANIFEST.md").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


require("ac911c74ded53d5f2fa6b7ad63c8c0f97fb9a395" in MANIFEST,
        "Stage 7A must pin the Stage 6.1 runtime base")
require("2f314cac6cc65f5664dc3254ece140bb68fb5390" in MANIFEST,
        "Stage 7A must pin the frozen hardened Pass 2 evidence SHA")
require("5b155937b8d05f0f0f9f1a02f10d9afe76a917d6035897695cce739eb8d6b1fd" in MANIFEST,
        "Stage 7A must pin the exact Atlas v2.6 corpus hash")

for source in (CATALOG, SESSION, CARDPUTER):
    require("reference_vocabulary" not in source.lower(),
            "temporary Stage 7A runtime must not depend on production ReferenceVocabulary")
    require("PAT_" not in source and "SRC_" not in source,
            "Atlas raw pattern/source identifiers must not enter firmware audition code")
    require("structural_group_id" not in source and "source_locator" not in source,
            "rights-sensitive Atlas lineage must remain offline")

require("RhythmRole::BassRhythm" not in CATALOG,
        "Stage 7A catalog must not introduce BassRhythm")
require("RhythmRole::ChordRhythm" not in CATALOG,
        "Stage 7A catalog must not introduce ChordRhythm")
require("RhythmRole::MelodicRhythm" not in CATALOG,
        "Stage 7A catalog must not introduce MelodicRhythm")
require("phraseBarsBit(1)" in CATALOG,
        "Stage 7A must remain one-bar to isolate archetype listening")
require("kStatementTrajectory" in CATALOG,
        "Stage 7A must stay Statement-only and avoid BarEvolution production wiring")

require("backupDrums_ = drums" in SESSION and
        "backupA_ = synthA" in SESSION and
        "backupB_ = synthB" in SESSION,
        "Stage 7A activation must snapshot current patterns")
require("drums = backupDrums_" in SESSION and
        "synthA = backupA_" in SESSION and
        "synthB = backupB_" in SESSION,
        "Stage 7A exit must restore exact current patterns")
require("synthA = SynthPattern{}" in SESSION and
        "synthB = SynthPattern{}" in SESSION,
        "Stage 7A listening must isolate drums by clearing temporary synth patterns")

require("event.alt && event.ctrl" in CARDPUTER and "key == 'a'" in CARDPUTER,
        "Stage 7A activation must require Ctrl+Alt+A")
require("engine.songModeEnabled()" in CARDPUTER,
        "Stage 7A activation must be refused in Song mode")
require("!event.ctrl || event.alt || event.meta" in CARDPUTER,
        "Stage 7A modal commands must use Ctrl without Alt/meta")
require("key >= '1' && key <= '5'" in CARDPUTER,
        "Stage 7A must expose exactly five numbered listening slots")
require("key == 'p'" in CARDPUTER and "key == '['" in CARDPUTER and
        "key == ']'" in CARDPUTER and "key == 'r'" in CARDPUTER,
        "Stage 7A seed/P-level/rerender controls must remain available")
require("key == 'b'" not in CARDPUTER and "key == 'B'" not in CARDPUTER,
        "Stage 7A must not revive the Stage 3A fixed-root Bass toggle")
require("(panicChord || key == ' ')" in CARDPUTER,
        "Stage 7A must release Space and panic to existing global owners")
require("engine.start()" not in CARDPUTER and "engine.stop()" not in CARDPUTER,
        "Stage 7A must not own transport start/stop directly")

require('#include "../../generation/audition_stage7/stage7a_cardputer.h"' in GENRE_PAGE,
        "GenrePage must include the temporary Stage 7A handler")
require("Stage7AAudition::handleCardputerEvent" in GENRE_PAGE,
        "GenrePage must give Stage 7A first refusal before normal Genre editing")
require(GENRE_PAGE.index("Stage7AAudition::handleCardputerEvent") < GENRE_PAGE.index("UIInput::isTab(event)"),
        "Stage 7A must intercept its modal commands before Genre navigation")

require("audition_stage7" not in MAIN_SKETCH,
        "Stage 7A must not modify the top-level sketch event loop")

print("Stage 7A source regressions: OK")
