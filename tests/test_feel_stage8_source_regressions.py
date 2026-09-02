#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


interpreter = (ROOT / "src/generation/feel/feel_interpreter.cpp").read_text()
adapter = (ROOT / "src/generation/feel/feel_pattern_adapter.cpp").read_text()
types = (ROOT / "src/generation/feel/feel_types.h").read_text()
live_bridge = (ROOT / "src/generation/migration/strong_rhythm_live_bridge.cpp").read_text()
engine = (ROOT / "src/dsp/miniacid_engine.cpp").read_text()
feel_ui = (ROOT / "src/ui/pages/feel_page.cpp").read_text()

require("Genre" not in interpreter and "GenerativeMode" not in interpreter,
        "FeelInterpreter must remain Genre-agnostic")
require("rand(" not in interpreter and "random(" not in interpreter,
        "FeelInterpreter must use the deterministic generation domain")
for transport_token in ("MIDI_CLOCK", "sendStart", "sendStop", "advanceTick"):
    require(transport_token not in interpreter,
            f"FeelInterpreter must not own transport token {transport_token}")
require("events[kMaxFeelEvents]" in types,
        "Phrase timing boundary must stay fixed-capacity")
require("GenerationDomain::FeelExpression" in interpreter,
        "Feel expression requires its isolated deterministic domain")
swing_compatible_case = interpreter.split(
    "case FeelProfileId::SwingCompatible:", 1)[1].split(
        "case FeelProfileId::LaidBack:", 1)[0]
require("step &" not in swing_compatible_case,
        "SwingCompatible must not encode a second odd-step swing law")
require("barOrigin" in interpreter and "idealOnTick" in interpreter,
        "Feel targets must derive from absolute ideal coordinates")
require("timingProfile" in live_bridge and "feelAmount" in live_bridge,
        "ordinary generation must carry persisted Feel intent")
require("applyFeelToMaterializedPattern" in adapter,
        "semantic plan must feed the physical pattern adapter")
require("timingProfile" not in engine,
        "transport/playback engine must not become a second Feel interpreter")
require('"PROFILE"' in feel_ui and "FeelProfileId::Count" in feel_ui,
        "existing FEEL workflow must expose every stable profile")

print("Generation Stage 8 source regressions: OK")
