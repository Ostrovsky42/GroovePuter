#!/usr/bin/env python3
from pathlib import Path

ROOT = Path.cwd()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    scenes = (ROOT / "scenes.cpp").read_text(encoding="utf-8")
    build = (ROOT / "scripts/build.sh").read_text(encoding="utf-8")
    midi_only = (ROOT / "scripts/build_seqtrak_midi_only.sh").read_text(encoding="utf-8")
    budget = (ROOT / "scripts/check_cardputer_dram_budget.sh").read_text(encoding="utf-8")

    require("#include <esp_attr.h>" in scenes,
            "Cardputer Scene storage must use the ESP external-BSS attribute")
    require("#error \"Cardputer firmware requires PSRAM for Scene storage\"" in scenes,
            "Cardputer builds must fail closed when PSRAM storage is unavailable")
    require("static GROOVEPUTER_SCENE_EXT_BSS Scene g_mainScene;" in scenes,
            "the active Scene must not consume fixed internal DRAM")
    require("static GROOVEPUTER_SCENE_EXT_BSS Scene s_tempLoadScene;" in scenes,
            "the transaction scratch Scene must not consume fixed internal DRAM")
    require("heap_caps_malloc" not in scenes and "ps_malloc" not in scenes,
            "Scene storage must remain static and fragmentation-free")

    for name, text in (("normal", build), ("SEQTRAK MIDI-only", midi_only)):
        require("PSRAM=enabled" in text, f"{name} Cardputer profile must enable PSRAM")
        require("PSRAM=disabled" not in text, f"{name} Cardputer profile must not disable PSRAM")

    require('MAX_BYTES="${2:-122880}"' in budget,
            "the fixed internal DRAM acceptance limit must remain 122880 bytes")
    print("Cardputer fixed DRAM Scene storage regressions: OK")


if __name__ == "__main__":
    main()
