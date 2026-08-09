#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STAGE = ROOT / "src" / "generation" / "audition_stage7"
CATALOG = (STAGE / "stage7a_catalog.cpp").read_text()
CATALOG_H = (STAGE / "stage7a_catalog.h").read_text()
SESSION = (STAGE / "stage7a_session.cpp").read_text()
CARD_H = (STAGE / "stage7a_cardputer.h").read_text()
CARD = (STAGE / "stage7a_cardputer.cpp").read_text()
DISPLAY = (ROOT / "src/ui/miniacid_display.cpp").read_text()
GENRE = (ROOT / "src/ui/pages/genre_page.cpp").read_text()
MAKEFILE = (ROOT / "platform_sdl/Makefile").read_text()


def require(value, message):
    if not value:
        raise AssertionError(message)


for candidate in ("HARD_01", "HARD_06", "HARD_07", "HARD_08"):
    require(candidate in CATALOG, f"missing {candidate}")
for candidate in ("HARD_02", "HARD_03", "HARD_04", "HARD_05", "HARD_09"):
    require(candidate not in CATALOG, f"Batch 1 candidate leaked into Stage 7B: {candidate}")

require("StackedQuarters" in CATALOG_H and "ElectroBackskip" in CATALOG_H and
        "FunkHouseBridge" in CATALOG_H and "ElectroGapPush" in CATALOG_H,
        "Stage 7B must expose exactly the remaining four candidate keys")
require(CATALOG.count("EvidenceClass::SingleRootChallenger") == 4,
        "all Stage 7B definitions must remain single-root challengers")
require("kFunkHouseBridgeRelationships" not in CATALOG,
        "HARD_07 must not freeze contradictory Kick/Backbeat relation")
require("kElectroBackskipRelationships" in CATALOG and
        "kElectroGapPushRelationships" in CATALOG,
        "HARD_06/HARD_08 must retain supported exclusion")

for text in (CATALOG, SESSION, CARD_H, CARD):
    require("reference_vocabulary" not in text.lower(),
            "temporary audition must not depend on production ReferenceVocabulary")
    require("structural_group_id" not in text and "source_locator" not in text,
            "Atlas source lineage must remain offline")

require("RhythmRole::BassRhythm" not in CATALOG and
        "RhythmRole::ChordRhythm" not in CATALOG and
        "RhythmRole::MelodicRhythm" not in CATALOG,
        "Stage 7B remains drums-only")
require("phraseBarsBit(1)" in CATALOG and "kStatementTrajectory" in CATALOG,
        "Stage 7B remains one-bar Statement-only")
require("archetype(711" in CATALOG and "archetype(714" in CATALOG,
        "temporary Stage 7B IDs must remain 711..714")

require("stage7a_session.h" not in CARD_H and "miniacid_engine.h" not in CARD_H and
        "transport_clock_runtime.h" not in CARD_H,
        "Cardputer facade header must stay lightweight")
require("stage7a_cardputer.cpp" in MAKEFILE,
        "SDL must link out-of-line Cardputer facade")
require("key >= '1' && key <= '4'" in CARD,
        "Stage 7B must expose exactly four slots")
require("UIInput::navCode(event)" in CARD and "nav == GROOVEPUTER_LEFT" in CARD and
        "nav == GROOVEPUTER_RIGHT" in CARD,
        "seed navigation must use normalized Ctrl+Left/Right")
require("key == '['" not in CARD and "key == ']'" not in CARD,
        "bracket characters must not own seed navigation")
require("TransportClockSource::SeqtrakExternal" in CARD,
        "Space must retain SEQ master guard")

panic = CARD[CARD.index("const bool panicChord"):CARD.index("if (toggleChord)")]
require("session.deactivate" in panic and panic.index("session.deactivate") < panic.index("return false"),
        "panic must restore/deactivate before global project reset")

active = DISPLAY.index("Stage7AAudition::cardputerSession().active()")
global_shortcut = DISPLAY.index("if (event.meta &&", active)
require("Stage7AAudition::handleCardputerEvent" in DISPLAY[active:global_shortcut],
        "active audition must get first refusal before global shortcuts")
require("Stage7AAudition::handleCardputerEvent" in GENRE,
        "Genre page must retain local activation owner")

require("backupDrums_ = drums" in SESSION and "drums = backupDrums_" in SESSION and
        "backupA_ = synthA" in SESSION and "synthA = backupA_" in SESSION and
        "backupB_ = synthB" in SESSION and "synthB = backupB_" in SESSION,
        "session must preserve exact backup/restore")
require("candidateIndex_ = oldIndex" in SESSION and "seed_ = oldSeed" in SESSION and
        "level_ = oldLevel" in SESSION and "identity_ = oldIdentity" in SESSION,
        "failed commands must roll back state")

print("Stage 7B source regressions: OK")
