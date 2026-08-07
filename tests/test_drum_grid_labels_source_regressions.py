#!/usr/bin/env python3
from pathlib import Path

ROOT = Path.cwd()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    grid = (ROOT / "src/ui/components/drum_sequencer_grid.cpp").read_text(
        encoding="utf-8"
    )

    require(
        'constexpr int kLaneLabelWidth = 34;' in grid
        and 'constexpr int kStepHeaderHeight = 8;' in grid
        and 'layout.grid_x = bounds.x + labelWidth;' in grid
        and 'layout.accent_y = bounds.y + kStepHeaderHeight;' in grid,
        "drum grid must reserve independent left-label and top-step-number areas",
    )
    require(
        '{"KICK", "SNARE", "HAT1", "HAT2", "PERC1", "PERC2", "RIM", "CLAP"}' in grid,
        "the established eight drum voices must have semantic lane labels",
    )
    require(
        'if (miniAcid.currentDrumEngineName() == "606")' in grid
        and 'if (voice == 6) return "CYM";' in grid
        and 'if (voice == 7) return "--";' in grid,
        "TR-606-specific lane meaning must remain explicit",
    )
    require(
        'void drawStepNumbers(' in grid
        and 'std::snprintf(label, sizeof(label), "%d", step + 1);' in grid
        and grid.count('drawStepNumbers(gfx') == 3,
        "all visual styles must show step numbers 1 through 16",
    )
    require(
        'drawAccentLabel(gfx' in grid
        and grid.count('drawAccentLabel(gfx') == 3,
        "the accent row must stay identifiable after adding lane labels",
    )
    require(
        'if (ui_event.x < layout.grid_x || ui_event.x >= layout.grid_right) return false;' in grid,
        "clicks in the new label column must never toggle step 1",
    )
    require(
        'const int availableW = std::max(1, bounds.w - labelWidth);' in grid
        and 'layout.cell_w = availableW / SEQ_STEPS;' in grid,
        "sixteen step cells must fit in the space remaining after the label column",
    )

    print("Drum grid labels source regressions: OK")


if __name__ == "__main__":
    main()
