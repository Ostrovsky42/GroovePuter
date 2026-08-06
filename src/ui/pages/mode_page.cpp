#include "mode_page.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "../axis_page_palette.h"
#include "../layout_manager.h"
#include "../ui_common.h"
#include "../ui_input.h"
#include "../../dsp/atlas_runtime.h"
#include "../../dsp/phrase_generator.h"

namespace {
uint8_t atlasVariationForRole(PhraseGenerator::PhraseBarRole role) {
  switch (role) {
    case PhraseGenerator::PhraseBarRole::Base:
    case PhraseGenerator::PhraseBarRole::Return:
      return 0;
    case PhraseGenerator::PhraseBarRole::MicroVariation:
    case PhraseGenerator::PhraseBarRole::Development:
    case PhraseGenerator::PhraseBarRole::Build:
      return 1;
    case PhraseGenerator::PhraseBarRole::Breakdown:
    case PhraseGenerator::PhraseBarRole::Fill:
    case PhraseGenerator::PhraseBarRole::EndingFill:
      return 2;
  }
  return 0;
}

int wrapIndex(int value, int count) {
  if (count <= 0) return 0;
  while (value < 0) value += count;
  while (value >= count) value -= count;
  return value;
}
}  // namespace

ModePage::ModePage(IGfx& gfx,
                   MiniAcid& mini_acid,
                   AudioGuard audio_guard)
    : mini_acid_(mini_acid), audio_guard_(audio_guard) {
  (void)gfx;
  style_ = UI::currentStyle;
}

void ModePage::moveFocus(int delta) {
  int value = static_cast<int>(focus_) + delta;
  value = wrapIndex(value, 3);
  focus_ = static_cast<FocusRow>(value);
}

void ModePage::shiftPhraseLength(int delta) {
  static constexpr uint8_t kLengths[4] = {1, 2, 4, 8};
  int index = 0;
  for (int i = 0; i < 4; ++i) {
    if (kLengths[i] == phrase_bars_) {
      index = i;
      break;
    }
  }
  index = wrapIndex(index + delta, 4);
  phrase_bars_ = kLengths[index];
}

const char* ModePage::planName() const {
  switch (phrase_bars_) {
    case 1: return "S";
    case 2: return "S > X";
    case 4: return "S > R > V > X";
    case 8: return "S R m V B K F X";
    default: return "S > X";
  }
}

void ModePage::generatePhrase() {
  const bool wasPlaying = mini_acid_.isPlaying();
  if (wasPlaying) mini_acid_.stop();

  PhraseGenerator::PhraseResult result{};
  withAudioGuard([&]() {
    PhraseGenerator::PhraseRequest request{};
    request.bars = phrase_bars_;
    request.songStart = std::max(0, mini_acid_.currentSongPosition());
    request.pageIndex = mini_acid_.currentPageIndex();
    request.seed = static_cast<uint32_t>(rand());

    Scene& scene = mini_acid_.sceneManager().currentScene();
    const GenreRecipeId recipe = mini_acid_.genreManager().recipe();
    const bool atlasPhrase = AtlasRuntime::hasRecipe(recipe) &&
                             AtlasRuntime::variationCount(recipe) >= 3;

    if (atlasPhrase) {
      result = PhraseGenerator::generateBarsToSong(
          scene, request,
          [&](PhraseGenerator::PhraseBar& bar,
              PhraseGenerator::PhraseBarRole role,
              int barIndex) {
            (void)barIndex;
            return AtlasRuntime::applyRecipe(
                recipe, atlasVariationForRole(role),
                bar.synthA, bar.synthB, bar.drums, nullptr);
          });
    } else {
      result = PhraseGenerator::generateToSong(
          scene, request, [&](PhraseGenerator::PhraseBar& base) {
            const GenerativeParams& params =
                mini_acid_.genreManager().getCompiledGenerativeParams();
            const GenreBehavior behavior = mini_acid_.genreManager().getBehavior();
            mini_acid_.modeManager().generatePattern(
                base.synthA, mini_acid_.bpm(), params, behavior, 0);
            mini_acid_.modeManager().generatePattern(
                base.synthB, mini_acid_.bpm(), params, behavior, 1);
            mini_acid_.modeManager().generateDrumPattern(
                base.drums, params, behavior);
          });
    }

    if (result) {
      mini_acid_.setSongMode(true);
      mini_acid_.setSongPlaybackSlot(scene.activeSongSlot);
      mini_acid_.setSongPosition(result.songStart);
    }
  });

  if (result) {
    char toast[80];
    std::snprintf(toast, sizeof(toast),
                  "%dB materialized -> Song %d..%d",
                  result.bars, result.songStart + 1,
                  result.songStart + result.bars);
    UI::showToast(toast, 1800);
  } else {
    UI::showToast(PhraseGenerator::errorText(result.error), 1600);
  }

  if (wasPlaying) mini_acid_.start();
}

void ModePage::draw(IGfx& gfx) {
  const AxisUI::Palette palette = AxisUI::paletteFor(style_);
  const IGfxColor axisColor = palette.generation;
  const Scene& scene = mini_acid_.sceneManager().currentScene();
  const GenerativeParams& params =
      mini_acid_.genreManager().getCompiledGenerativeParams();

  UI::drawStandardHeader(gfx, mini_acid_, "GENERATION");
  LayoutManager::clearContent(gfx);

  const int x = Layout::COL_1;
  const int width = Layout::CONTENT.w - Layout::CONTENT_PAD_X * 2;
  AxisUI::drawAxisTag(gfx, x, LayoutManager::lineY(0),
                      "GEN 3/4", "FORM / DEVELOPMENT",
                      axisColor, palette);

  char value[96];
  std::snprintf(value, sizeof(value), "%s / %s",
                GenreManager::generativeModeName(
                    mini_acid_.genreManager().generativeMode()),
                GenreManager::recipeName(mini_acid_.genreManager().recipe()));
  gfx.setTextColor(palette.muted);
  gfx.drawText(x + 2, LayoutManager::lineY(1) + 1, value);

  std::snprintf(value, sizeof(value), "%u BARS",
                static_cast<unsigned>(phrase_bars_));
  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(2), width,
                       "LENGTH", value,
                       focus_ == FocusRow::Length,
                       axisColor, palette);

  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(3), width,
                       "PLAN", planName(), false,
                       axisColor, palette);

  std::snprintf(value, sizeof(value), "SONG %c ROW %d",
                static_cast<char>('A' + std::clamp(scene.activeSongSlot, 0, 1)),
                std::max(0, mini_acid_.currentSongPosition()) + 1);
  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(4), width,
                       "TARGET", value,
                       focus_ == FocusRow::Target,
                       axisColor, palette);

  std::snprintf(value, sizeof(value), "ENTER / G  %uB",
                static_cast<unsigned>(phrase_bars_));
  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(5), width,
                       "MATERIALIZE", value,
                       focus_ == FocusRow::Materialize,
                       axisColor, palette);

  std::snprintf(value, sizeof(value),
                "A %.0f%%  S %.0f%%  FILL %.0f%%",
                params.accentProbability * 100.0f,
                params.slideProbability * 100.0f,
                params.fillProbability * 100.0f);
  gfx.setTextColor(palette.muted);
  gfx.drawText(x + 2, LayoutManager::lineY(6) + 1, value);

  gfx.setTextColor(palette.text);
  gfx.drawText(x + 2, LayoutManager::lineY(7) + 1,
               "Linear constructive pass / no retry");

  UI::drawStandardFooter(gfx,
                         "TAB/U/D:FIELD  L/R:LENGTH",
                         "ENTER/G:MATERIALIZE TO SONG");
}

bool ModePage::handleEvent(UIEvent& event) {
  if (event.event_type != GROOVEPUTER_KEY_DOWN) return false;

  if (UIInput::isTab(event)) {
    moveFocus(1);
    return true;
  }

  const int nav = UIInput::navCode(event);
  if (nav == GROOVEPUTER_UP) {
    moveFocus(-1);
    return true;
  }
  if (nav == GROOVEPUTER_DOWN) {
    moveFocus(1);
    return true;
  }
  if (nav == GROOVEPUTER_LEFT || nav == GROOVEPUTER_RIGHT) {
    if (focus_ == FocusRow::Length) {
      shiftPhraseLength(nav == GROOVEPUTER_RIGHT ? 1 : -1);
    }
    return true;
  }

  if (event.key == '\n' || event.key == '\r') {
    if (focus_ == FocusRow::Length) shiftPhraseLength(1);
    else if (focus_ == FocusRow::Materialize) generatePhrase();
    return true;
  }

  if ((event.key == 'g' || event.key == 'G') &&
      !event.ctrl && !event.alt && !event.meta) {
    generatePhrase();
    return true;
  }

  return false;
}
