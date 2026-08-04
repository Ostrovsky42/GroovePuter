#!/usr/bin/env python3
from pathlib import Path

ROOT = Path.cwd()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    scenes = (ROOT / "scenes.cpp").read_text(encoding="utf-8")
    header = (ROOT / "scenes.h").read_text(encoding="utf-8")
    sketch = (ROOT / "GroovePuter.ino").read_text(encoding="utf-8")
    engine = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")
    build = (ROOT / "scripts/build.sh").read_text(encoding="utf-8")
    midi_only = (ROOT / "scripts/build_seqtrak_midi_only.sh").read_text(encoding="utf-8")
    budget = (ROOT / "scripts/check_cardputer_dram_budget.sh").read_text(encoding="utf-8")

    require("bool initializeSceneStorage();" in header,
            "SceneManager must expose explicit post-PSRAM storage initialization")
    require("heap_caps_malloc(" in scenes,
            "Cardputer Scene storage must use explicit capability allocation")
    require("MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT" in scenes,
            "Scene storage must require external byte-addressable RAM")
    require("SceneStorageBlock* g_sceneStorageBlock = nullptr;" in scenes,
            "Cardputer fixed BSS must retain only the Scene storage pointer")
    require("static Scene g_mainScene" not in scenes,
            "the active Scene must not return to fixed internal BSS")
    require("static Scene s_tempLoadScene" not in scenes,
            "the transaction Scene must not return to fixed internal BSS")
    require("EXT_RAM_BSS_ATTR" not in scenes,
            "the M5Stack Arduino profile must not rely on disabled external-BSS linking")
    require("initializeSceneStorage()" in sketch,
            "Cardputer setup must initialize Scene storage explicitly")
    require(sketch.index("initializeSceneStorage()") > sketch.index("M5Cardputer.begin(cfg)"),
            "Scene PSRAM allocation must happen after board initialization")
    require(sketch.index("initializeSceneStorage()") < sketch.index("g_audioOut.begin"),
            "Scene PSRAM allocation must happen before realtime runtime startup")
    require("fatal-scene-psram-allocation" in sketch,
            "Cardputer must fail closed when Scene PSRAM is unavailable")
    require("#if defined(ARDUINO_M5STACK_CARDPUTER)" in engine and
            "hasPsram = false;" in engine,
            "enabling Scene PSRAM must not silently change the Cardputer audio profile")

    for name, text in (("normal", build), ("SEQTRAK MIDI-only", midi_only)):
        require("PSRAM=enabled" in text, f"{name} Cardputer profile must enable PSRAM")
        require("PSRAM=disabled" not in text, f"{name} Cardputer profile must not disable PSRAM")

    require('MAX_BYTES="${2:-122880}"' in budget,
            "the fixed internal DRAM acceptance limit must remain 122880 bytes")
    print("Cardputer fixed DRAM Scene storage regressions: OK")


if __name__ == "__main__":
    main()
