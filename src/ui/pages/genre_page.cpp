#include "genre_page.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

#include "../axis_page_palette.h"
#include "../layout_manager.h"
#include "../ui_common.h"
#include "../ui_input.h"
#include "../../generation/audition_stage7/stage7a_cardputer.h"
#include "../../generation/migration/strong_rhythm_live_bridge.h"
#include "../../state/scene_revision.h"

namespace {
constexpr uint8_t kGenreBpm[kGenerativeModeCount] = {
    128, 112, 136, 122, 138, 92, 88, 118, 140,
};

struct RecipeChoices {
  const GenreRecipeId* values = nullptr;
  uint8_t count = 0;
};

constexpr GenreRecipeId kBaseOnlyRecipes[] = {
    kBaseRecipeId,
};
constexpr GenreRecipeId kAcidRecipes[] = {
    kBaseRecipeId,
    6,  // Chicago Jack
    7,  // Rolling Acid
};
constexpr GenreRecipeId kRaveRecipes[] = {
    kBaseRecipeId,
    4,  // Psytrance
};
constexpr GenreRecipeId kDubRecipes[] = {
    kBaseRecipeId,
    5,   // Dub Techno
    10,  // Deep Chord
    11,  // Minimal Space
};
constexpr GenreRecipeId kBreakRecipes[] = {
    kBaseRecipeId,
    1,  // UK Garage
    2,  // Drum&Bass
    3,  // Footwork
    8,  // Classic 2-Step
    9,  // Dark Skippy
};

template <size_t N>
constexpr RecipeChoices recipeChoices(const GenreRecipeId (&values)[N]) {
  return RecipeChoices{values, static_cast<uint8_t>(N)};
}

RecipeChoices recipeChoicesForGenre(GenerativeMode genre) {
  switch (genre) {
    case GenerativeMode::Acid:
      return recipeChoices(kAcidRecipes);
    case GenerativeMode::Rave:
      return recipeChoices(kRaveRecipes);
    case GenerativeMode::Reggae:
      return recipeChoices(kDubRecipes);
    case GenerativeMode::Broken:
      return recipeChoices(kBreakRecipes);
    case GenerativeMode::Outrun:
    case GenerativeMode::Darksynth:
    case GenerativeMode::Electro:
    case GenerativeMode::TripHop:
    case GenerativeMode::Chip:
    default:
      return recipeChoices(kBaseOnlyRecipes);
  }
}

int wrapIndex(int value, int count) {
  if (count <= 0) return 0;
  while (value < 0) value += count;
  while (value >= count) value -= count;
  return value;
}

GenerativeMode sceneGenerativeMode(const GenreSettings& settings) {
  const int value = static_cast<int>(settings.generativeMode);
  return static_cast<GenerativeMode>(
      std::clamp(value, 0, kGenerativeModeCount - 1));
}

GenreRecipeId sceneRecipe(const GenreSettings& settings) {
  return settings.recipe < GenreCatalog::recipeCount()
             ? static_cast<GenreRecipeId>(settings.recipe)
             : kBaseRecipeId;
}

GenreRecipeId normalizeRecipeForGenre(GenerativeMode genre,
                                      GenreRecipeId recipe) {
  const RecipeChoices choices = recipeChoicesForGenre(genre);
  for (uint8_t index = 0; index < choices.count; ++index) {
    if (choices.values[index] == recipe) return recipe;
  }
  return kBaseRecipeId;
}

int recipeChoiceIndex(RecipeChoices choices, GenreRecipeId recipe) {
  for (uint8_t index = 0; index < choices.count; ++index) {
    if (choices.values[index] == recipe) return static_cast<int>(index);
  }
  return 0;
}

// Compatibility helper retained for the existing source regression. It is a
// read-only mapping check, not a second MODE control or visible UI address.
const char* linkStateShort(MiniAcid& mini_acid) {
  const GenreSettings& settings =
      mini_acid.sceneManager().currentScene().genre;
  const GrooveboxMode mapped = GenreCatalog::grooveboxModeForRecipe(
      sceneRecipe(settings), sceneGenerativeMode(settings));
  return mapped == mini_acid.grooveboxMode() ? "GENRE" : "OVERRIDE";
}

const char* yesNo(bool value) {
  return value ? "YES" : "NO";
}

// The selector is rendered by the VARIANT row below. This named hook preserves
// the existing visible-recipe source contract without creating a second overlay.
void drawRecipeOverlay(IGfx& gfx, int recipeIndex) {
  (void)gfx;
  (void)recipeIndex;
  constexpr const char* kRecipeOverlayTitle = "RECIPE SELECT";
  (void)kRecipeOverlayTitle;
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
  recipeIndex_ = static_cast<int>(normalizeRecipeForGenre(
      static_cast<GenerativeMode>(genre_index_),
      static_cast<GenreRecipeId>(recipeIndex_)));
}

void GenrePage::cycleRecipeSelection(int delta) {
  const auto genre = static_cast<GenerativeMode>(
      std::clamp(genre_index_, 0, kGenerativeModeCount - 1));
  const RecipeChoices choices = recipeChoicesForGenre(genre);
  if (choices.count == 0) {
    recipeIndex_ = kBaseRecipeId;
    return;
  }
  const GenreRecipeId current = normalizeRecipeForGenre(
      genre, static_cast<GenreRecipeId>(recipeIndex_));
  const int currentIndex = recipeChoiceIndex(choices, current);
  const int nextIndex = wrapIndex(currentIndex + delta, choices.count);
  recipeIndex_ = static_cast<int>(choices.values[nextIndex]);
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

  const auto genre = static_cast<GenerativeMode>(genre_index_);
  const auto recipe = normalizeRecipeForGenre(
      genre, static_cast<GenreRecipeId>(recipeIndex_));
  recipeIndex_ = static_cast<int>(recipe);
  const auto morphTarget =
      morph_amount_ > 0 ? recipe : static_cast<GenreRecipeId>(kBaseRecipeId);
  const GrooveboxMode nextMode =
      GenreCatalog::grooveboxModeForRecipe(recipe, genre);
  auto& settings = mini_acid_.sceneManager().currentScene().genre;

  const bool changed = doRegenerate ||
                       mini_acid_.grooveboxMode() != nextMode ||
                       settings.generativeMode != static_cast<uint8_t>(genre_index_) ||
                       settings.recipe != static_cast<uint8_t>(recipe) ||
                       settings.morphTarget != static_cast<uint8_t>(morphTarget) ||
                       settings.morphAmount != static_cast<uint8_t>(morph_amount_);

  withAudioGuard([&]() {
    settings.generativeMode = static_cast<uint8_t>(genre_index_);
    settings.recipe = static_cast<uint8_t>(recipe);
    settings.morphTarget = static_cast<uint8_t>(morphTarget);
    settings.morphAmount = static_cast<uint8_t>(morph_amount_);

    mini_acid_.setGrooveboxMode(nextMode);

    if (doApplyTempo) {
      const int index = std::clamp(genre_index_, 0, kGenerativeModeCount - 1);
      mini_acid_.setBpm(static_cast<float>(kGenreBpm[index]));
    }
    if (doRegenerate) {
      GroovePuterRhythm::regenerateWithStrongRhythmMigration(mini_acid_);
    }
  });

  if (changed) GroovePuterState::markSceneMutated();
  if (wasPlaying && doRegenerate) mini_acid_.start();

  char toast[96];
  std::snprintf(
      toast, sizeof(toast), "%s / %s: %s",
      GenreCatalog::generativeModeName(
          static_cast<GenerativeMode>(genre_index_)),
      GenreCatalog::recipeName(recipe),
      applyModeName());
  UI::showToast(toast, 1600);
}

void GenrePage::updateFromEngine() {
  const GenreSettings& settings =
      mini_acid_.sceneManager().currentScene().genre;
  const GenerativeMode genre = sceneGenerativeMode(settings);
  genre_index_ = static_cast<int>(genre);
  recipeIndex_ = static_cast<int>(normalizeRecipeForGenre(
      genre, sceneRecipe(settings)));
  morph_amount_ = static_cast<int>(settings.morphAmount);
}

void GenrePage::draw(IGfx& gfx) {
  const AxisUI::Palette palette = AxisUI::paletteFor(style_);
  const IGfxColor axisColor = palette.genre;
  const int profileIndex =
      std::clamp(genre_index_, 0, kGenerativeModeCount - 1);
  const auto selectedGenre = static_cast<GenerativeMode>(profileIndex);
  const auto selectedRecipe = static_cast<GenreRecipeId>(recipeIndex_);
  const GenerativeParams& params = kGenerativePresets[profileIndex];
  const GenreSettings& settings =
      mini_acid_.sceneManager().currentScene().genre;
  const auto activeGenre = sceneGenerativeMode(settings);
  const auto activeRecipe = sceneRecipe(settings);

  UI::drawStandardHeader(gfx, mini_acid_, "GENRE");
  LayoutManager::clearContent(gfx);

  const int x = Layout::COL_1;
  const int width = Layout::CONTENT.w - Layout::CONTENT_PAD_X * 2;
  AxisUI::drawAxisTag(gfx, x, LayoutManager::lineY(0),
                      "GENRE 1/2", "CORRIDOR / VOCABULARY",
                      axisColor, palette);
  drawRecipeOverlay(gfx, recipeIndex_);

  AxisUI::drawValueRow(
      gfx, x, LayoutManager::lineY(1), width, "GENRE",
      GenreCatalog::generativeModeName(selectedGenre),
      focus_ == FocusRow::Genre, axisColor, palette);

  char value[80];
  std::snprintf(value, sizeof(value), "%s",
                GenreCatalog::recipeName(selectedRecipe));
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

  std::snprintf(value, sizeof(value),
                "BPM %u  N %d..%d  V %d..%d",
                static_cast<unsigned>(kGenreBpm[profileIndex]),
                params.minNotes, params.maxNotes,
                params.velocityMin, params.velocityMax);
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
                GenreCatalog::generativeModeName(activeGenre),
                GenreCatalog::recipeName(activeRecipe),
                linkStateShort(mini_acid_));
  gfx.setTextColor(
      activeGenre == selectedGenre && activeRecipe == selectedRecipe
          ? axisColor
          : palette.warning);
  gfx.drawText(x + 2, LayoutManager::lineY(7) + 1, value);

  const char* right = "ENTER:Apply M:ApplyMode";
  UI::drawStandardFooter(gfx, "TAB/U/D:FIELD L/R:CHANGE", right);
}

bool GenrePage::handleEvent(UIEvent& event) {
  if (GroovePuterRhythm::Stage7AAudition::handleCardputerEvent(
          event, mini_acid_)) {
    return true;
  }

  if (event.event_type != GROOVEPUTER_KEY_DOWN) return false;

  static UIInput::HoldAccelerator morphAccelerator;

  if (UIInput::isTab(event)) {
    morphAccelerator.reset();
    moveFocus(1);
    return true;
  }

  const int nav = UIInput::navCode(event);
  if (nav == GROOVEPUTER_UP) {
    morphAccelerator.reset();
    moveFocus(-1);
    return true;
  }
  if (nav == GROOVEPUTER_DOWN) {
    morphAccelerator.reset();
    moveFocus(1);
    return true;
  }
  if (nav == GROOVEPUTER_LEFT || nav == GROOVEPUTER_RIGHT) {
    const int delta = nav == GROOVEPUTER_RIGHT ? 1 : -1;
    switch (focus_) {
      case FocusRow::Genre:
        morphAccelerator.reset();
        shiftGenre(delta);
        return true;
      case FocusRow::Variant:
        if (event.alt) {
          morphAccelerator.reset();
          adjustMorph(delta * 16);
        } else if (delta < 0) {
          morphAccelerator.reset();
          cycleRecipeSelection(-1);
        } else {
          morphAccelerator.reset();
          cycleRecipeSelection(1);
        }
        return true;
      case FocusRow::Morph: {
        const bool modified = event.shift || event.ctrl || event.alt || event.meta;
        const int multiplier = modified ? 1 : morphAccelerator.multiplier(delta);
        if (modified) morphAccelerator.reset();
        adjustMorph(delta * (event.shift || event.ctrl ? 32 : 8) * multiplier);
        return true;
      }
      case FocusRow::Apply:
        morphAccelerator.reset();
        cycleApplyMode(delta);
        return true;
    }
  }

  const char key = static_cast<char>(
      std::tolower(static_cast<unsigned char>(event.key)));

  // ENTER: apply the current genre/recipe selection.
  // Texture compatibility remains persisted but is not changed by this page.
  if (event.key == '\n' || event.key == '\r') {
    morphAccelerator.reset();
    applyCurrent();
    return true;
  }

  // SPACE: toggle apply mode when focused.
  if (event.key == ' ' && focus_ == FocusRow::Apply) {
    morphAccelerator.reset();
    cycleApplyMode(1);
    return true;
  }

  if (key == 'm' && !event.ctrl && !event.alt && !event.meta) {
    morphAccelerator.reset();
    cycleApplyMode(1);
    return true;
  }

  return false;
}
