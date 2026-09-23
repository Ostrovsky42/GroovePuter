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
        'constexpr int kLaneLabelWidth = 24;' in grid
        and 'constexpr int kStepHeaderHeight = 8;' in grid
        and 'layout.grid_x = bounds.x + labelWidth;' in grid
        and 'layout.grid_y = layout.accent_y;' in grid,
        "drum grid must reserve numbered lane labels and the top step header",
    )
    require(
        '{"3KIK", "4SNR", "5HH1", "6HH2", "7PR1", "8PR2", "9RIM", "0CLP"}' in grid,
        "drum lanes must expose their matching global mute digits",
    )
    require(
        'if (miniAcid.currentDrumEngineName() == "606")' in grid
        and 'if (voice == 6) return "9CYM";' in grid
        and 'if (voice == 7) return "0---";' in grid,
        "TR-606-specific lane meaning must remain explicit",
    )
    require(
        'void drawStepNumbers(' in grid
        and "label[0] = static_cast<char>('0' + ((step + 1) % 10));" in grid
        and grid.count('drawStepNumbers(gfx') == 3,
        "all visual styles must use compact one-glyph step headers 1..9,0..6",
    )
    require(
        'drawAccentLabel(gfx' not in grid
        and 'hit && stepData.accent' in grid
        and 'bounds.h - kStepHeaderHeight' in grid,
        "accent must be rendered per hit and the removed ACC row must return height to all eight lanes",
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
