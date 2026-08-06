#include "genre_page.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

#include "../axis_page_palette.h"
#include "../layout_manager.h"
#include "../ui_common.h"
#include "../ui_input.h"

namespace {
constexpr uint8_t kGenreBpm[kGenerativeModeCount] = {
    128, 112, 136, 122, 138, 92, 88, 118, 140,
};

int wrapIndex(int value, int count) {
  if (count <= 0) return 0;
  while (value < 0) value += count;
  while (value >= count) value -= count;
  return value;
}

// Compatibility helper retained for the existing source regression. It is a
// read-only mapping check, not a second MODE control or visible UI address.
const char* linkStateShort(MiniAcid& mini_acid) {
  const GrooveboxMode mapped = GenreManager::grooveboxModeForRecipe(
      mini_acid.genreManager().recipe(),
      mini_acid.genreManager().generativeMode());
  return mapped == mini_acid.grooveboxMode() ? "GENRE" : "OVERRIDE";
}

int clampRecipeIndex(int value) {
  const int count = static_cast<int>(GenreManager::recipeCount());
  return count > 0 ? wrapIndex(value, count) : 0;
}

const char* yesNo(bool value) {
  return value ? "YES" : "NO";
}
}  // namespace

GenrePage::GenrePage(IGfx& gfx,
                     MiniAcid& mini_acid,
                     AudioGuard audio_guard)
    : mini_acid_(mini_acid), audio_guard_(audio_guard) {
  (void)gfx;
  style_ = UI::currentStyle;
  updateFromEngine();
}

GenrePage::ApplyMode GenrePage::currentApplyMode() const {
  const auto& settings = mini_acid_.sceneManager().currentScene().genre;
  if (!settings.regenerateOnApply) return ApplyMode::ProfileOnly;
  if (settings.applyTempoOnApply) return ApplyMode::RegenerateTempo;
  return ApplyMode::Regenerate;
}

const char* GenrePage::applyModeName() const {
  switch (currentApplyMode()) {
    case ApplyMode::ProfileOnly: return "PROFILE ONLY";
    case ApplyMode::Regenerate: return "MATERIALIZE";
    case ApplyMode::RegenerateTempo: return "MATERIALIZE+BPM";
  }
  return "PROFILE ONLY";
}

void GenrePage::moveFocus(int delta) {
  constexpr int kCount = 4;
  int value = static_cast<int>(focus_) + delta;
  value = wrapIndex(value, kCount);
  focus_ = static_cast<FocusRow>(value);
}

void GenrePage::shiftGenre(int delta) {
  genre_index_ = wrapIndex(genre_index_ + delta, kGenerativeModeCount);
}

void GenrePage::shiftVariant(int delta) {
  const int count = static_cast<int>(GenreManager::recipeCount());
  if (count <= 0) return;
  recipe_index_ = wrapIndex(recipe_index_ + delta, count);
}

void GenrePage::adjustMorph(int delta) {
  morph_amount_ = std::clamp(morph_amount_ + delta, 0, 255);
}

void GenrePage::cycleApplyMode(int delta) {
  int value = static_cast<int>(currentApplyMode()) + delta;
  value = wrapIndex(value, 3);
  const ApplyMode next = static_cast<ApplyMode>(value);
  auto& settings = mini_acid_.sceneManager().currentScene().genre;
  settings.regenerateOnApply = next != ApplyMode::ProfileOnly;
  settings.applyTempoOnApply = next == ApplyMode::RegenerateTempo;
  GroovePuterState::markSceneMutated();
}

void GenrePage::applyCurrent() {
  const ApplyMode applyMode = currentApplyMode();
  const bool doRegenerate = applyMode != ApplyMode::ProfileOnly;
  const bool doApplyTempo = applyMode == ApplyMode::RegenerateTempo;
  const bool wasPlaying = mini_acid_.isPlaying();
  if (wasPlaying && doRegenerate) mini_acid_.stop();

  withAudioGuard([&]() {
    const auto genre = static_cast<GenerativeMode>(genre_index_);
    const auto recipe = static_cast<GenreRecipeId>(recipe_index_);
    const auto morphTarget =
        morph_amount_ > 0 ? recipe : static_cast<GenreRecipeId>(kBaseRecipeId);

    auto& manager = mini_acid_.genreManager();
    manager.setGenerativeMode(genre);
    manager.setRecipe(recipe);
    manager.setMorphTarget(morphTarget);
    manager.setMorphAmount(static_cast<uint8_t>(morph_amount_));

    mini_acid_.setGrooveboxMode(
        GenreManager::grooveboxModeForRecipe(recipe, genre));

    auto& settings = mini_acid_.sceneManager().currentScene().genre;
    settings.generativeMode = static_cast<uint8_t>(genre_index_);
    settings.recipe = static_cast<uint8_t>(recipe_index_);
    settings.morphTarget = static_cast<uint8_t>(morphTarget);
    settings.morphAmount = static_cast<uint8_t>(morph_amount_);

    if (doApplyTempo) {
      const int index = std::clamp(genre_index_, 0, kGenerativeModeCount - 1);
      mini_acid_.setBpm(static_cast<float>(kGenreBpm[index]));
    }
    if (doRegenerate) mini_acid_.regeneratePatternsWithGenre();
  });

  if (wasPlaying && doRegenerate) mini_acid_.start();

  char toast[96];
  std::snprintf(
      toast, sizeof(toast), "%s / %s: %s",
      GenreManager::generativeModeName(
          static_cast<GenerativeMode>(genre_index_)),
      GenreManager::recipeName(static_cast<GenreRecipeId>(recipe_index_)),
      applyModeName());
  UI::showToast(toast, 1600);
}

void GenrePage::updateFromEngine() {
  genre_index_ = static_cast<int>(mini_acid_.genreManager().generativeMode());
  recipe_index_ = clampRecipeIndex(
      static_cast<int>(mini_acid_.genreManager().recipe()));
  morph_amount_ = static_cast<int>(mini_acid_.genreManager().morphAmount());
}

void GenrePage::draw(IGfx& gfx) {
  const AxisUI::Palette palette = AxisUI::paletteFor(style_);
  const IGfxColor axisColor = palette.genre;
  const auto selectedGenre = static_cast<GenerativeMode>(genre_index_);
  const auto selectedRecipe = static_cast<GenreRecipeId>(recipe_index_);
  const GenerativeParams& params =
      mini_acid_.genreManager().getCompiledGenerativeParams();
  const GrooveRecipe recipe = mini_acid_.genreManager().getGrooveRecipe();
  const auto activeGenre = mini_acid_.genreManager().generativeMode();
  const auto activeRecipe = mini_acid_.genreManager().recipe();

  UI::drawStandardHeader(gfx, mini_acid_, "GENRE");
  LayoutManager::clearContent(gfx);

  const int x = Layout::COL_1;
  const int width = Layout::CONTENT.w - Layout::CONTENT_PAD_X * 2;
  AxisUI::drawAxisTag(gfx, x, LayoutManager::lineY(0),
                      "GENRE 1/4", "CORRIDOR / VOCABULARY",
                      axisColor, palette);

  AxisUI::drawValueRow(
      gfx, x, LayoutManager::lineY(1), width, "GENRE",
      GenreManager::generativeModeName(selectedGenre),
      focus_ == FocusRow::Genre, axisColor, palette);

  char value[80];
  std::snprintf(value, sizeof(value), "%s",
                GenreManager::recipeName(selectedRecipe));
  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(2), width, "VARIANT",
                       value, focus_ == FocusRow::Variant,
                       axisColor, palette);

  std::snprintf(value, sizeof(value), "%d%%",
                (morph_amount_ * 100) / 255);
  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(3), width, "MORPH",
                       value, focus_ == FocusRow::Morph,
                       axisColor, palette);

  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(4), width, "APPLY",
                       applyModeName(), focus_ == FocusRow::Apply,
                       axisColor, palette);

  const int index = std::clamp(genre_index_, 0, kGenerativeModeCount - 1);
  std::snprintf(value, sizeof(value),
                "BPM %u  GRID %u  N %d..%d",
                static_cast<unsigned>(kGenreBpm[index]),
                static_cast<unsigned>(recipe.stepsPerBar),
                params.minNotes, params.maxNotes);
  gfx.setTextColor(palette.muted);
  gfx.drawText(x + 2, LayoutManager::lineY(5) + 1, value);

  std::snprintf(value, sizeof(value),
                "DOWNBEAT %s  KICK %s  HATS %s",
                yesNo(params.preferDownbeats),
                params.sparseKick ? "SPARSE" : "OPEN",
                params.sparseHats ? "SPARSE" : "OPEN");
  gfx.setTextColor(palette.muted);
  gfx.drawText(x + 2, LayoutManager::lineY(6) + 1, value);

  std::snprintf(value, sizeof(value), "ACTIVE %s/%s MAP:%s",
                GenreManager::generativeModeName(activeGenre),
                GenreManager::recipeName(activeRecipe),
                linkStateShort(mini_acid_));
  gfx.setTextColor(
      activeGenre == selectedGenre && activeRecipe == selectedRecipe
          ? axisColor
          : palette.warning);
  gfx.drawText(x + 2, LayoutManager::lineY(7) + 1, value);

  UI::drawStandardFooter(
      gfx, "TAB/U/D:FIELD  L/R:CHANGE",
      "ALT+L/R:MORPH  ENTER:APPLY");
}

bool GenrePage::handleEvent(UIEvent& event) {
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
    const int delta = nav == GROOVEPUTER_RIGHT ? 1 : -1;
    switch (focus_) {
      case FocusRow::Genre:
        shiftGenre(delta);
        return true;
      case FocusRow::Variant:
        if (event.alt) adjustMorph(delta * 16);
        else shiftVariant(delta);
        return true;
      case FocusRow::Morph:
        adjustMorph(delta * (event.shift || event.ctrl ? 32 : 8));
        return true;
      case FocusRow::Apply:
        cycleApplyMode(delta);
        return true;
    }
  }

  const char key = static_cast<char>(
      std::tolower(static_cast<unsigned char>(event.key)));
  if (event.key == '\n' || event.key == '\r') {
    applyCurrent();
    return true;
  }
  if (key == 'm' && !event.ctrl && !event.alt && !event.meta) {
    cycleApplyMode(1);
    return true;
  }

  return false;
}
