#include "generation_page.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "../axis_page_palette.h"
#include "../layout_manager.h"
#include "../ui_common.h"
#include "../../dsp/atlas_runtime.h"
#include "../../dsp/genre_sparse_repair.h"
#include "../../dsp/genre_variant_catalog.h"
#include "../../dsp/phrase_generator.h"
#include "src/state/scene_revision.h"

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

void formatPatternLabel(int globalPattern, char* out, int outSize) {
  if (!out || outSize <= 0 || globalPattern < 0) return;
  std::snprintf(out, outSize, "%d%c%d",
                songPatternPage(globalPattern) + 1,
                static_cast<char>('A' + songPatternBank(globalPattern)),
                songPatternIndexInBank(globalPattern) + 1);
}

int clampSongRow(int row) {
  return std::clamp(row, 0, Song::kMaxPositions - 1);
}
}  // namespace

GenerationPage::GenerationPage(IGfx& gfx,
                               MiniAcid& mini_acid,
                               AudioGuard audio_guard)
    : mini_acid_(mini_acid), audio_guard_(audio_guard) {
  (void)gfx;
  style_ = UI::currentStyle;
  syncTargetFromSong();
}

void GenerationPage::onEnter(int context) {
  (void)context;
  syncTargetFromSong();
}

void GenerationPage::syncTargetFromSong() {
  target_row_ = clampSongRow(mini_acid_.currentSongPosition());
  hold_accel_.reset();
  last_attempted_ = false;
}

void GenerationPage::moveTargetRow(int delta, bool fast) {
  if (delta == 0) return;

  const int previousRow = target_row_;
  const int multiplier = hold_accel_.multiplier(delta, fast);
  target_row_ = clampSongRow(target_row_ + delta * multiplier);
  last_attempted_ = false;

  if (target_row_ == previousRow) return;

  Serial.printf("[GENERATION] target %d -> %d delta=%d mult=%d\n",
                previousRow + 1, target_row_ + 1, delta, multiplier);
  char toast[48];
  std::snprintf(toast, sizeof(toast), "GEN TARGET ROW %d", target_row_ + 1);
  UI::showToast(toast, 650);
}

// Ownership contract: ROW OCCUPIED:BLOCK  LEN:PHRASE
void GenerationPage::materializeCurrentBar() {
  const bool wasPlaying = mini_acid_.isPlaying();
  if (wasPlaying) mini_acid_.stop();

  PhraseGenerator::PhraseResult result{};
  const int targetRow = clampSongRow(target_row_);
  Serial.printf("[GENERATION] write request row=%d\n", targetRow + 1);
  const auto generate = [&]() {
    PhraseGenerator::PhraseRequest request{};
    request.bars = kMaterializeBars;
    request.songStart = targetRow;
    request.pageIndex = mini_acid_.currentPageIndex();
    request.seed = static_cast<uint32_t>(rand());

    Scene& scene = mini_acid_.sceneManager().currentScene();
    auto& genreManager = mini_acid_.genreManager();
    const GenerativeMode activeGenre = genreManager.generativeMode();
    const GenreRecipeId recipe = genreManager.recipe();
    const bool atlasPhrase = AtlasRuntime::hasRecipe(recipe) &&
                             AtlasRuntime::variationCount(recipe) >= 3;

    if (atlasPhrase) {
      result = PhraseGenerator::generateBarsToSong(
          scene, request,
          [&](PhraseGenerator::PhraseBar& bar,
              PhraseGenerator::PhraseBarRole role,
              int barIndex) {
            (void)barIndex;
            const uint8_t variation = atlasVariationForRole(role);
            const bool applied = AtlasRuntime::applyRecipe(
                recipe, variation,
                bar.synthA, bar.synthB, bar.drums, nullptr);
            if (applied) {
              GenreSparseRepair::applySparseLeadContract(
                  activeGenre, recipe, variation, bar.synthB);
            }
            return applied;
          });
    } else {
      result = PhraseGenerator::generateToSong(
          scene, request, [&](PhraseGenerator::PhraseBar& base) {
            const GenerativeParams& params =
                genreManager.getCompiledGenerativeParams();
            const GenreBehavior behavior = genreManager.getBehavior();
            GrooveboxModeManager generator(mini_acid_);
            generator.setModeLocal(GenreManager::grooveboxModeForRecipe(
                recipe, activeGenre));
            generator.setFlavorLocal(0);
            generator.setGenerationSeed(request.seed);
            generator.generatePattern(
                base.synthA, mini_acid_.bpm(), params, behavior, 0);
            generator.generatePattern(
                base.synthB, mini_acid_.bpm(), params, behavior, 1);
            GenreSparseRepair::applySparseLeadContract(
                activeGenre, recipe, 0, base.synthB);
            generator.generateDrumPattern(base.drums, params, behavior);
          });
    }

    if (result) {
      mini_acid_.setSongMode(true);
      mini_acid_.setSongPlaybackSlot(scene.activeSongSlot);
      mini_acid_.setSongPosition(result.songStart);
    }
  };

  if (audio_guard_) audio_guard_(generate);
  else generate();

  last_attempted_ = true;
  last_success_ = static_cast<bool>(result);
  last_row_ = targetRow;
  last_pattern_ = result.firstGlobalPattern;

  if (result) {
    GroovePuterState::markSceneMutated();
    target_row_ = clampSongRow(result.songStart);
    char patternLabel[16] = "---";
    formatPatternLabel(result.firstGlobalPattern,
                       patternLabel, sizeof(patternLabel));
    char status[64];
    std::snprintf(status, sizeof(status), "OK ROW %d -> %s",
                  result.songStart + 1, patternLabel);
    last_status_ = status;

    char toast[112];
    std::snprintf(toast, sizeof(toast), "GEN OK %s/%s ROW %d -> %s",
                  GenreVariantCatalog::genreDisplayName(
                      mini_acid_.genreManager().generativeMode()),
                  GenreVariantCatalog::recipeDisplayName(
                      mini_acid_.genreManager().recipe()),
                  result.songStart + 1, patternLabel);
    UI::showToast(toast, 2000);
    Serial.printf("[GENERATION] write ok row=%d pattern=%d\n",
                  result.songStart + 1, result.firstGlobalPattern);
  } else {
    last_status_ = PhraseGenerator::errorText(result.error);
    char toast[96];
    std::snprintf(toast, sizeof(toast), "GEN BLOCKED ROW %d: %s",
                  targetRow + 1, last_status_.c_str());
    UI::showToast(toast, 2200);
    Serial.printf("[GENERATION] write blocked row=%d error=%s\n",
                  targetRow + 1, last_status_.c_str());
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
                      "GEN 3/4", "WRITE ONE SONG BAR",
                      axisColor, palette);

  char value[112];
  std::snprintf(value, sizeof(value), "%s / %s",
                GenreVariantCatalog::genreDisplayName(
                    mini_acid_.genreManager().generativeMode()),
                GenreVariantCatalog::recipeDisplayName(
                    mini_acid_.genreManager().recipe()));
  gfx.setTextColor(palette.muted);
  gfx.drawText(x + 2, LayoutManager::lineY(1) + 1, value);

  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(2), width,
                       "SCOPE", "CURRENT EMPTY SONG ROW", false,
                       axisColor, palette);
  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(3), width,
                       "PLAN", "SINGLE BAR / BASE", false,
                       axisColor, palette);

  std::snprintf(value, sizeof(value), "SONG %c ROW %d",
                static_cast<char>('A' + std::clamp(scene.activeSongSlot, 0, 1)),
                target_row_ + 1);
  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(4), width,
                       "TARGET", value, true,
                       axisColor, palette);

  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(5), width,
                       "WRITE", "ENTER / G", false,
                       axisColor, palette);

  std::snprintf(value, sizeof(value),
                "A %.0f%%  S %.0f%%  FILL %.0f%%",
                params.accentProbability * 100.0f,
                params.slideProbability * 100.0f,
                params.fillProbability * 100.0f);
  gfx.setTextColor(palette.muted);
  gfx.drawText(x + 2, LayoutManager::lineY(6) + 1, value);

  std::snprintf(value, sizeof(value), "LAST %s",
                last_attempted_ ? last_status_.c_str() : "READY");
  gfx.setTextColor(!last_attempted_ || last_success_
                       ? axisColor
                       : palette.warning);
  gfx.drawText(x + 2, LayoutManager::lineY(7) + 1, value);

  UI::drawStandardFooter(gfx,
                         "L/R:+-1  U/D:+-8",
                         "HOLD L/R:ACCEL  ENTER/G:WRITE");
}

bool GenerationPage::handleEvent(UIEvent& event) {
  if (event.event_type != GROOVEPUTER_KEY_DOWN) return false;

  const int nav = UIInput::navCode(event);
  if (nav == GROOVEPUTER_LEFT) {
    moveTargetRow(-1, event.shift || event.ctrl || event.alt);
    return true;
  }
  if (nav == GROOVEPUTER_RIGHT) {
    moveTargetRow(1, event.shift || event.ctrl || event.alt);
    return true;
  }
  if (nav == GROOVEPUTER_UP || nav == GROOVEPUTER_DOWN) {
    hold_accel_.reset();
    const int previousRow = target_row_;
    target_row_ = clampSongRow(
        target_row_ + (nav == GROOVEPUTER_DOWN ? 8 : -8));
    last_attempted_ = false;

    if (target_row_ != previousRow) {
      Serial.printf("[GENERATION] target %d -> %d jump=%d\n",
                    previousRow + 1, target_row_ + 1,
                    nav == GROOVEPUTER_DOWN ? 8 : -8);
      char toast[48];
      std::snprintf(toast, sizeof(toast),
                    "GEN TARGET ROW %d", target_row_ + 1);
      UI::showToast(toast, 650);
    }
    return true;
  }

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
