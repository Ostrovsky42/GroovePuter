#!/usr/bin/env python3
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class UiLayoutSourceRegressions(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def test_pattern_editor_derives_rows_from_step_count(self):
        source = self.read("src/ui/pages/pattern_edit_page.cpp")
        self.assertIn("constexpr int kPatternStepColumns = 8;", source)
        self.assertIn("constexpr int kPatternStepRows", source)
        self.assertIn("SEQ_STEPS <= 32", source)
        self.assertIn("dst_max_row >= kPatternStepRows", source)
        self.assertIn("spacing * (kPatternStepRows - 1)", source)
        self.assertNotIn("newRow > 1", source)
        self.assertNotIn("tr > 1 || tc < 0 || tc > 7", source)

    def test_song_has_native_retro_and_amber_split_renderers(self):
        source = self.read("src/ui/pages/song_page.cpp")
        self.assertNotIn(
            "split_compare_ && visual_style_ != ::VisualStyle::RETRO_CLASSIC",
            source,
        )
        self.assertIn("void SongPage::drawRetroClassicStyle", source)
        self.assertIn("void SongPage::drawAmberStyle", source)
        self.assertGreaterEqual(source.count("EDIT:%c  PLAY:%c"), 2)
        self.assertGreaterEqual(source.count("const int gap = 4;"), 2)
        self.assertIn("UI::themePalette(::VisualStyle::MINIMAL)", source)
        self.assertGreaterEqual(source.count("const int gap = 4;"), 3)
        self.assertNotIn("const int cursorRow = cursorRow();", source)

    def test_settings_and_hub_use_explicit_top_insets(self):
        synth = self.read("src/ui/pages/synth_sequencer_page.cpp")
        drums = self.read("src/ui/pages/drum_sequencer_page.cpp")
        hub = self.read("src/ui/pages/sequencer_hub_page.cpp")
        self.assertIn("const int y = content.y + 7;", synth)
        self.assertIn("int y = bounds.y + 5;", drums)
        self.assertIn("constexpr int kHubOverviewTopInset = 8;", hub)
        self.assertGreaterEqual(hub.count("kHubOverviewTopInset"), 5)


if __name__ == "__main__":
    unittest.main()
