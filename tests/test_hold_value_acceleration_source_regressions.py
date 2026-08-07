from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    ui_input = (ROOT / "src/ui/ui_input.h").read_text(encoding="utf-8")
    genre = (ROOT / "src/ui/pages/genre_page.cpp").read_text(encoding="utf-8")
    synth = (ROOT / "src/ui/pages/tb303_params_page.cpp").read_text(encoding="utf-8")
    drums = (ROOT / "src/ui/pages/drum_automation_page.cpp").read_text(encoding="utf-8")

    require("class HoldAccelerator" in ui_input,
            "shared held-value accelerator is missing")
    require("x1 -> x2 -> x3 -> x4" in ui_input and
            "if (streak_ >= 24) return 4;" in ui_input and
            "if (streak_ >= 14) return 3;" in ui_input and
            "if (streak_ >= 6) return 2;" in ui_input,
            "held-value acceleration must use the softer bounded ramp")

    require("HoldAccelerator morphAccelerator" in genre and
            "morphAccelerator.multiplier" in genre,
            "GENRE Morph must retain held-value acceleration")
    require("HoldAccelerator knobAccelerator" in synth and
            "knobAccelerator.multiplier" in synth,
            "Synth continuous parameters must retain held-value acceleration")
    require("HoldAccelerator holdAccelerator" in drums and
            "holdAccelerator.multiplier" in drums,
            "Drum Automation continuous values must retain held-value acceleration")

    require("if (modified) morphAccelerator.reset();" in genre,
            "modified GENRE editing must remain precise")
    require("const int multiplier = fine ? 1 : knobAccelerator.multiplier" in synth,
            "Synth fine editing must remain unaccelerated")
    require("isContinuousRow()" in drums,
            "Drum Automation must accelerate only continuous rows")

    print("held value acceleration source regressions: OK")


if __name__ == "__main__":
    main()
