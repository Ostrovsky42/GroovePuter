#!/usr/bin/env python3
from pathlib import Path

ROOT = Path.cwd()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    scenes = (ROOT / "scenes.cpp").read_text(encoding="utf-8")
    header = (ROOT / "scenes.h").read_text(encoding="utf-8")
    paging = (ROOT / "src/audio/pattern_paging.cpp").read_text(encoding="utf-8")
    sketch = (ROOT / "GroovePuter.ino").read_text(encoding="utf-8")
    build = (ROOT / "scripts/build.sh").read_text(encoding="utf-8")
    midi_only = (ROOT / "scripts/build_seqtrak_midi_only.sh").read_text(encoding="utf-8")
    budget = (ROOT / "scripts/check_cardputer_dram_budget.sh").read_text(encoding="utf-8")

    require("bool initializeSceneStorage();" in header,
  "SceneManager must expose explicit post-board storage initialization")
    require("class SceneScratchLease" in header,
  "transaction Scene must have bounded RAII lifetime")
    require("MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT" in scenes,
  "Cardputer Scene storage must use byte-addressable internal heap")
    require("MALLOC_CAP_SPIRAM" not in scenes,
  "Cardputer Scene storage must not require unavailable PSRAM")
    require("Scene* g_mainScene = nullptr;" in scenes,
  "fixed BSS must retain only the active Scene pointer")
    require("static Scene g_mainScene" not in scenes,
  "the active Scene must not return to fixed internal BSS")
    require("static Scene s_tempLoadScene" not in scenes,
  "the transaction Scene must not return to fixed internal BSS")
    require("SceneScratchLease scratch;" in scenes,
  "project loading must release transaction storage on every return")
    require(paging.count("SceneScratchLease scratch;") == 2,
  "pattern-page save and load must use bounded transaction storage")
    require("initializeSceneStorage()" in sketch,
  "Cardputer setup must initialize active Scene storage explicitly")
    require(sketch.index("initializeSceneStorage()") > sketch.index("M5Cardputer.begin(cfg)"),
  "active Scene allocation must happen after board initialization")
    require(sketch.index("initializeSceneStorage()") < sketch.index("g_audioOut.begin"),
  "active Scene allocation must happen before realtime startup")
    require("fatal-scene-internal-allocation" in sketch and "FATAL: SCENE RAM" in sketch,
  "allocation failure must be visible without CDC")

    for name, text in (("normal", build), ("SEQTRAK MIDI-only", midi_only)):
        require("PSRAM=disabled" in text, f"{name} Cardputer profile must disable PSRAM")
        require("PSRAM=enabled" not in text, f"{name} Cardputer profile must not claim PSRAM")

    require('MAX_BYTES="${2:-122880}"' in budget,
  "the fixed internal DRAM acceptance limit must remain 122880 bytes")
    print("Cardputer fixed DRAM Scene storage regressions: OK")


if __name__ == "__main__":
    main()
