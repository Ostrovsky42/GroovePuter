#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_once(path: Path, old: str, new: str) -> bool:
    text = path.read_text(encoding="utf-8")
    if new in text:
        return False
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected exactly one anchor, found {count}: {old!r}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")
    return True


def main() -> None:
    changed = False

    sketch = ROOT / "GroovePuter.ino"
    changed |= replace_once(
        sketch,
        '#include "src/diag/melody_pending_census.h"\n',
        '#include "src/diag/melody_pending_census.h"\n'
        '#include "src/diag/lifetime_census.h"\n',
    )
    changed |= replace_once(
        sketch,
        '  markBootStage(100, "setup-complete");\n',
        '  markBootStage(100, "setup-complete");\n'
        '  LIFETIME_CENSUS_POINT("BOOT", "product-ready");\n',
    )

    smf = ROOT / "src/platform/cardputer_smf_player.cpp"
    changed |= replace_once(
        smf,
        '#include "src/audio/audio_config.h"\n',
        '#include "src/audio/audio_config.h"\n'
        '#include "src/diag/lifetime_census.h"\n',
    )
    changed |= replace_once(
        smf,
        '    Serial.printf("[SMF-INIT] ready freeInt=%u largest=%u\\n",\n'
        '                  static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),\n'
        '                  static_cast<unsigned>(\n'
        '                      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));\n'
        '    return true;\n',
        '    Serial.printf("[SMF-INIT] ready freeInt=%u largest=%u\\n",\n'
        '                  static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),\n'
        '                  static_cast<unsigned>(\n'
        '                      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));\n'
        '    LIFETIME_CENSUS_POINT("SMF_PLAY", "after-create");\n'
        '    return true;\n',
    )
    changed |= replace_once(
        smf,
        'void CardputerSmfPlayerService::taskLoop() {\n'
        '    while (true) {\n',
        'void CardputerSmfPlayerService::taskLoop() {\n'
        '    LIFETIME_CENSUS_POINT("SMF_PLAY", "worker-start");\n'
        '    while (true) {\n',
    )

    print("LIFETIME-R1 L1 hooks materialized" if changed else
          "LIFETIME-R1 L1 hooks already present")


if __name__ == "__main__":
    main()
