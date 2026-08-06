#include "generation_page.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "../axis_page_palette.h"
#include "../layout_manager.h"
#include "../ui_common.h"
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
}  // namespace

GenerationPage::GenerationPage(IGfx& gfx,
                     MiniAcid& mini_acid,
                     AudioGuard audio_guard)
    : mini_acid_(mini_acid), audio_guard_(audio_guard) {
  (void)gfx;
  style_ = UI::currentStyle;
}

void GenerationPage::materializeCurrentBar() {
  const bool wasPlaying = mini_acid_.isPlaying();
  if (wasPlaying) mini_acid_.stop();

  PhraseGenerator::PhraseResult result{};
  withAudioGuard([&]() {
    PhraseGenerator::PhraseRequest request{};
    request.bars = kMaterializeBars;
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
    char toast[64];
    std::snprintf(toast, sizeof(toast),
        "BAR materialized -> Song %d",
        result.songStart + 1);
    UI::showToast(toast, 1800);
  } else {
    UI::showToast(PhraseGenerator::errorText(result.error), 1600);
  }

  if (wasPlaying) mini_acid_.start();
}

void GenerationPage::draw(IGfx& gfx) {
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

  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(2), width,
             "SCOPE", "CURRENT SONG ROW", false,
             axisColor, palette);
  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(3), width,
             "PLAN", "SINGLE BAR / BASE", false,
             axisColor, palette);

  std::snprintf(value, sizeof(value), "SONG %c ROW %d",
      static_cast<char>('A' + std::clamp(scene.activeSongSlot, 0, 1)),
      std::max(0, mini_acid_.currentSongPosition()) + 1);
  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(4), width,
             "TARGET", value, false,
             axisColor, palette);

  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(5), width,
             "MATERIALIZE", "ENTER / G", true,
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
     "Phrase length owned by PHRASE CORE");

  UI::drawStandardFooter(gfx,
               "ENTER/G:MATERIALIZE 1 BAR",
               "PHRASE LEN:PHRASE CORE");
}

bool GenerationPage::handleEvent(UIEvent& event) {
  if (event.event_type != GROOVEPUTER_KEY_DOWN) return false;

  if (event.key == '\n' || event.key == '\r') {
    materializeCurrentBar();
    return true;
  }

  if ((event.key == 'g' || event.key == 'G') &&
      !event.ctrl && !event.alt && !event.meta) {
    materializeCurrentBar();
    return true;
  }

  return false;
}
