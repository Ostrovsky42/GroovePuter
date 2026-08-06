#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCENES_H = (ROOT / "scenes.h").read_text()
SCENES_CPP = (ROOT / "scenes.cpp").read_text()


def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        raise AssertionError(f"missing {label}: {needle}")


require(
    SCENES_H,
    '#include "src/phrase/phrase_types.h"',
    "persisted Phrase POD include",
)
require(
    SCENES_H,
    '#include "src/phrase/phrase_persistence.h"',
    "Phrase persistence include",
)
require(
    SCENES_H,
    "PhraseCore::PhraseBank phraseBank;",
    "Phrase bank in Scene",
)
require(
    SCENES_H,
    "PhraseCore,\n    CustomPhrases",
    "evented Phrase path",
)
require(
    SCENES_H,
    'writeLiteral(",\\\"phraseCore\\\":[")',
    "streaming writer Phrase field",
)
require(
    SCENES_H,
    "PhraseCore::persistentValueAt(scene_->phraseBank, i)",
    "streaming writer flat values",
)

require(
    SCENES_CPP,
    '#include "src/phrase/phrase_core.h"',
    "Phrase implementation include",
)
require(
    SCENES_CPP,
    "PhraseCore::reset(scene.phraseBank);",
    "Scene transaction reset",
)
if SCENES_CPP.count("PhraseCore::reset(scene_->phraseBank);") < 2:
    raise AssertionError("default/wipe Scene paths must both reset Phrase bank")
require(
    SCENES_CPP,
    'root["phraseCore"].to<ArduinoJson::JsonArray>()',
    "ArduinoJson document writer",
)
require(
    SCENES_CPP,
    'obj["phraseCore"].as<ArduinoJson::JsonArrayConst>()',
    "ArduinoJson document reader",
)
require(
    SCENES_CPP,
    "PhraseCore::kPersistValueCount",
    "exact flat codec size",
)
require(
    SCENES_CPP,
    'lastKey_ == "phraseCore"',
    "evented parser key",
)
require(
    SCENES_CPP,
    "PhraseCore::beginPersistentDecode(target_.phraseBank);",
    "evented decode reset",
)
require(
    SCENES_CPP,
    "PhraseCore::applyPersistentValue(\n            target_.phraseBank",
    "evented numeric decode",
)
require(
    SCENES_CPP,
    "PhraseCore::sanitize(target_.phraseBank);",
    "evented decode sanitize",
)

if "phraseCore" in SCENES_CPP and "std::vector" in SCENES_CPP:
    # Existing unrelated vectors are allowed; the Phrase integration itself must
    # stay the fixed flat array codec and never introduce a Phrase vector.
    forbidden = [
        "std::vector<PhraseCore",
        "std::vector<Phrase",
        "new PhraseCore",
        "new PhraseBank",
    ]
    for token in forbidden:
        if token in SCENES_CPP or token in SCENES_H:
            raise AssertionError(f"dynamic Phrase persistence is forbidden: {token}")

print("Phrase Scene source regressions passed")
