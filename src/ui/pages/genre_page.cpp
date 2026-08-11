#include "genre_page.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

#include "../axis_page_palette.h"
#include "../layout_manager.h"
#include "../ui_common.h"
#include "../ui_input.h"
#include "../../generation/composition/generation_profile.h"
#include "../../generation/migration/strong_rhythm_live_bridge.h"
#include "../../state/generation_request_state.h"
#include "../../state/scene_revision.h"

namespace {
struct RecipeChoices {
  const GenreRecipeId* values = nullptr;
  uint8_t count = 0;
};

constexpr GenreRecipeId kBaseOnlyRecipes[] = {kBaseRecipeId};
constexpr GenreRecipeId kAcidRecipes[] = {kBaseRecipeId, 6, 7};
constexpr GenreRecipeId kRaveRecipes[] = {kBaseRecipeId, 4};
constexpr GenreRecipeId kDubRecipes[] = {kBaseRecipeId, 5, 10, 11};
constexpr GenreRecipeId kBreakRecipes[] = {kBaseRecipeId, 1, 2, 3, 8, 9};
constexpr GenreRecipeId kLoFiRecipes[] = {
    kBaseRecipeId, kClassicChillRecipeId, kDrunkenGrooveRecipeId,
    kLoFiHouseRecipeId, kMinimalSleepRecipeId,
};
constexpr GenreRecipeId kHipHopRecipes[] = {
    kBaseRecipeId, kGoldenEraRecipeId, kDustyJazzRecipeId,
};

template <size_t N>
constexpr RecipeChoices recipeChoices(const GenreRecipeId (&values)[N]) {
  return RecipeChoices{values, static_cast<uint8_t>(N)};
}

RecipeChoices recipeChoicesForGenre(GenerativeMode genre) {
  switch (genre) {
    case GenerativeMode::Acid: return recipeChoices(kAcidRecipes);
    case GenerativeMode::Rave: return recipeChoices(kRaveRecipes);
    case GenerativeMode::Reggae: return recipeChoices(kDubRecipes);
    case GenerativeMode::Broken: return recipeChoices(kBreakRecipes);
    case GenerativeMode::LoFi: return recipeChoices(kLoFiRecipes);
    case GenerativeMode::HipHop: return recipeChoices(kHipHopRecipes);
    default: return recipeChoices(kBaseOnlyRecipes);
  }
}

int wrapIndex(int value, int count) {
  if (count <= 0) return 0;
  while (value < 0) value += count;
  while (value >= count) value -= count;
  return value;
}

GenerativeMode sceneGenerativeMode(const GenreSettings& settings) {
  return static_cast<GenerativeMode>(
      std::clamp(static_cast<int>(settings.generativeMode), 0,
                 kGenerativeModeCount - 1));
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

const char* linkStateShort(MiniAcid& mini_acid) {
  const GenreSettings& settings = mini_acid.sceneManager().currentScene().genre;
  const GrooveboxMode mapped = GenreCatalog::grooveboxModeForRecipe(
      sceneRecipe(settings), sceneGenerativeMode(settings));
  return mapped == mini_acid.grooveboxMode() ? "GENRE" : "OVERRIDE";
}

void drawRecipeOverlay(IGfx& gfx, int recipeIndex) {
  (void)gfx;
  (void)recipeIndex;
  constexpr const char* kRecipeOverlayTitle = "RECIPE SELECT";
  (void)kRecipeOverlayTitle;
}
}  // namespace

GenrePage::GenrePage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard)
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
  constexpr int kCount = 5;
  focus_ = static_cast<FocusRow>(wrapIndex(static_cast<int>(focus_) + delta, kCount));
}

void GenrePage::shiftGenre(int delta) {
  genre_index_ = wrapIndex(genre_index_ + delta, kGenerativeModeCount);
  recipeIndex_ = static_cast<int>(normalizeRecipeForGenre(
      static_cast<GenerativeMode>(genre_index_),
      static_cast<GenreRecipeId>(recipeIndex_)));
  normalizePendingRhythm(true);
}

void GenrePage::cycleRecipeSelection(int delta) {
  const auto genre = static_cast<GenerativeMode>(
      std::clamp(genre_index_, 0, kGenerativeModeCount - 1));
  const RecipeChoices choices = recipeChoicesForGenre(genre);
  const GenreRecipeId current = normalizeRecipeForGenre(
      genre, static_cast<GenreRecipeId>(recipeIndex_));
  const int currentIndex = recipeChoiceIndex(choices, current);
  recipeIndex_ = static_cast<int>(
      choices.values[wrapIndex(currentIndex + delta, choices.count)]);
  normalizePendingRhythm(true);
}

GenreSettings GenrePage::pendingSettings() const {
  GenreSettings settings = mini_acid_.sceneManager().currentScene().genre;
  settings.generativeMode = static_cast<uint8_t>(
      std::clamp(genre_index_, 0, kGenerativeModeCount - 1));
  settings.recipe = static_cast<uint8_t>(normalizeRecipeForGenre(
      static_cast<GenerativeMode>(settings.generativeMode),
      static_cast<GenreRecipeId>(recipeIndex_)));
  settings.rhythmSelectionMode = static_cast<uint8_t>(rhythmMode_);
  settings.rhythmArchetypeId = rhythmArchetypeId_;
  return settings;
}

bool GenrePage::normalizePendingRhythm(bool notify) {
  if (rhythmMode_ != GroovePuterRhythm::RhythmSelectionMode::Manual) {
    rhythmArchetypeId_ = GroovePuterRhythm::kNoArchetypeId;
    return false;
  }
  if (GroovePuterRhythm::isRhythmCompatible(
          pendingSettings(), rhythmArchetypeId_)) return false;
  rhythmMode_ = GroovePuterRhythm::RhythmSelectionMode::Auto;
  rhythmArchetypeId_ = GroovePuterRhythm::kNoArchetypeId;
  if (notify) UI::showToast("RHYTHM RESET TO AUTO", 1200);
  return true;
}

void GenrePage::cycleRhythmSelection(int delta) {
  const GenreSettings settings = pendingSettings();
  const uint8_t count = GroovePuterRhythm::compatibleRhythmCount(settings);
  if (count == 0) {
    rhythmMode_ = GroovePuterRhythm::RhythmSelectionMode::Auto;
    rhythmArchetypeId_ = GroovePuterRhythm::kNoArchetypeId;
    return;
  }
  int position = 0;
  if (rhythmMode_ == GroovePuterRhythm::RhythmSelectionMode::Manual) {
    for (uint8_t index = 0; index < count; ++index) {
      if (GroovePuterRhythm::compatibleRhythmId(settings, index) == rhythmArchetypeId_) {
        position = static_cast<int>(index) + 1;
        break;
      }
    }
  }
  position = wrapIndex(position + delta, static_cast<int>(count) + 1);
  if (position == 0) {
    rhythmMode_ = GroovePuterRhythm::RhythmSelectionMode::Auto;
    rhythmArchetypeId_ = GroovePuterRhythm::kNoArchetypeId;
  } else {
    rhythmMode_ = GroovePuterRhythm::RhythmSelectionMode::Manual;
    rhythmArchetypeId_ = GroovePuterRhythm::compatibleRhythmId(
        settings, static_cast<uint8_t>(position - 1));
  }
}

void GenrePage::adjustMorph(int delta) {
  morph_amount_ = std::clamp(morph_amount_ + delta, 0, 255);
}

void GenrePage::cycleApplyMode(int delta) {
  const ApplyMode next = static_cast<ApplyMode>(
      wrapIndex(static_cast<int>(currentApplyMode()) + delta, 3));
  auto& settings = mini_acid_.sceneManager().currentScene().genre;
  settings.regenerateOnApply = next != ApplyMode::ProfileOnly;
  settings.applyTempoOnApply = next == ApplyMode::RegenerateTempo;
  GroovePuterState::markSceneMutated();
}

void GenrePage::applyCurrent(bool forceRegenerate) {
  const ApplyMode applyMode = currentApplyMode();
  const bool doRegenerate = forceRegenerate || applyMode != ApplyMode::ProfileOnly;
  const bool doApplyTempo = applyMode == ApplyMode::RegenerateTempo;
  const bool wasPlaying = mini_acid_.isPlaying();
  if (wasPlaying && doRegenerate) mini_acid_.stop();

  const auto genre = static_cast<GenerativeMode>(genre_index_);
  const auto recipe = normalizeRecipeForGenre(
      genre, static_cast<GenreRecipeId>(recipeIndex_));
  recipeIndex_ = static_cast<int>(recipe);
  normalizePendingRhythm(true);
  const auto morphTarget =
      morph_amount_ > 0 ? recipe : static_cast<GenreRecipeId>(kBaseRecipeId);
  const GrooveboxMode nextMode = GenreCatalog::grooveboxModeForRecipe(recipe, genre);
  auto& settings = mini_acid_.sceneManager().currentScene().genre;

  const bool changed = doRegenerate || mini_acid_.grooveboxMode() != nextMode ||
      settings.generativeMode != static_cast<uint8_t>(genre_index_) ||
      settings.recipe != static_cast<uint8_t>(recipe) ||
      settings.morphTarget != static_cast<uint8_t>(morphTarget) ||
      settings.morphAmount != static_cast<uint8_t>(morph_amount_) ||
      settings.rhythmSelectionMode != static_cast<uint8_t>(rhythmMode_) ||
      settings.rhythmArchetypeId != rhythmArchetypeId_;

  withAudioGuard([&]() {
    settings.generativeMode = static_cast<uint8_t>(genre_index_);
    settings.recipe = static_cast<uint8_t>(recipe);
    settings.morphTarget = static_cast<uint8_t>(morphTarget);
    settings.morphAmount = static_cast<uint8_t>(morph_amount_);
    settings.rhythmSelectionMode = static_cast<uint8_t>(rhythmMode_);
    settings.rhythmArchetypeId = rhythmArchetypeId_;
    mini_acid_.setGrooveboxMode(nextMode);
    if (doApplyTempo) {
      const GroovePuterRhythm::GenerationProfileView profile =
          GroovePuterRhythm::generationProfileFor(settings);
      if (profile.corridor.suggestedBpm > 0)
        mini_acid_.setBpm(static_cast<float>(profile.corridor.suggestedBpm));
    }
    if (doRegenerate)
      GroovePuterRhythm::regenerateWithStrongRhythmMigration(mini_acid_);
  });

  if (changed) GroovePuterState::markSceneMutated();
  if (wasPlaying && doRegenerate) mini_acid_.start();

  char toast[96];
  std::snprintf(toast, sizeof(toast), "%s / %s: %s",
                GenreCatalog::generativeModeName(genre),
                GenreCatalog::recipeName(recipe),
                forceRegenerate ? "GENERATED" : applyModeName());
  UI::showToast(toast, 1600);
}

void GenrePage::updateFromEngine() {
  const GenreSettings& settings = mini_acid_.sceneManager().currentScene().genre;
  const GenerativeMode genre = sceneGenerativeMode(settings);
  genre_index_ = static_cast<int>(genre);
  recipeIndex_ = static_cast<int>(normalizeRecipeForGenre(genre, sceneRecipe(settings)));
  rhythmMode_ = settings.rhythmSelectionMode == static_cast<uint8_t>(
                    GroovePuterRhythm::RhythmSelectionMode::Manual)
                    ? GroovePuterRhythm::RhythmSelectionMode::Manual
                    : GroovePuterRhythm::RhythmSelectionMode::Auto;
  rhythmArchetypeId_ = settings.rhythmArchetypeId;
  rhythmFallbackPending_ = normalizePendingRhythm(false);
  morph_amount_ = static_cast<int>(settings.morphAmount);
}

void GenrePage::draw(IGfx& gfx) {
  if (rhythmFallbackPending_) {
    UI::showToast("RHYTHM RESET TO AUTO", 1200);
    rhythmFallbackPending_ = false;
  }
  const AxisUI::Palette palette = AxisUI::paletteFor(style_);
  const IGfxColor axisColor = palette.genre;
  const int profileIndex = std::clamp(genre_index_, 0, kGenerativeModeCount - 1);
  const auto selectedGenre = static_cast<GenerativeMode>(profileIndex);
  const auto selectedRecipe = static_cast<GenreRecipeId>(recipeIndex_);
  const GroovePuterRhythm::GenerationProfileView selectedProfile =
      GroovePuterRhythm::generationProfileFor(pendingSettings());
  const GenreSettings& settings = mini_acid_.sceneManager().currentScene().genre;
  const auto activeGenre = sceneGenerativeMode(settings);
  const auto activeRecipe = sceneRecipe(settings);

  UI::drawStandardHeader(gfx, mini_acid_, "GENRE");
  LayoutManager::clearContent(gfx);
  const int x = Layout::COL_1;
  const int width = Layout::CONTENT.w - Layout::CONTENT_PAD_X * 2;
  AxisUI::drawAxisTag(gfx, x, LayoutManager::lineY(0), "GENRE 1/2",
                      "CORRIDOR / VOCABULARY", axisColor, palette);
  drawRecipeOverlay(gfx, recipeIndex_);

  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(1), width, "GENRE",
      GenreCatalog::generativeModeName(selectedGenre),
      focus_ == FocusRow::Genre, axisColor, palette);

  char value[80];
  std::snprintf(value, sizeof(value), "%s", GenreCatalog::recipeName(selectedRecipe));
  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(2), width, "VARIANT", value,
                       focus_ == FocusRow::Variant, axisColor, palette);

  const char* rhythmName = "AUTO";
  if (rhythmMode_ == GroovePuterRhythm::RhythmSelectionMode::Manual) {
    const char* selectedName = GroovePuterRhythm::rhythmSelectionName(rhythmArchetypeId_);
    rhythmName = selectedName == nullptr ? "AUTO" : selectedName;
  }
  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(3), width, "RHYTHM", rhythmName,
                       focus_ == FocusRow::Rhythm, axisColor, palette);

  std::snprintf(value, sizeof(value), "%d%%", (morph_amount_ * 100) / 255);
  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(4), width, "MORPH", value,
                       focus_ == FocusRow::Morph, axisColor, palette);
  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(5), width, "APPLY",
                       applyModeName(), focus_ == FocusRow::Apply, axisColor, palette);

  std::snprintf(value, sizeof(value), "BPM %u (%u-%u) D%u..%u",
      static_cast<unsigned>(selectedProfile.corridor.suggestedBpm),
      static_cast<unsigned>(selectedProfile.corridor.bpmMin),
      static_cast<unsigned>(selectedProfile.corridor.bpmMax),
      static_cast<unsigned>(selectedProfile.corridor.densityMin),
      static_cast<unsigned>(selectedProfile.corridor.densityMax));
  gfx.setTextColor(palette.muted);
  gfx.drawText(x + 2, LayoutManager::lineY(6) + 1, value);

  std::snprintf(value, sizeof(value), "A:%s/%s %s %s",
      GenreCatalog::generativeModeName(activeGenre),
      GenreCatalog::recipeName(activeRecipe),
      linkStateShort(mini_acid_),
      GroovePuterState::generationLevelShortName(
          GroovePuterState::currentGenerationLevel()));
  gfx.setTextColor(activeGenre == selectedGenre && activeRecipe == selectedRecipe
                       ? axisColor : palette.warning);
  gfx.drawText(x + 2, LayoutManager::lineY(7) + 1, value);

  UI::drawStandardFooter(gfx, "TAB/U/D:FIELD L/R:CHANGE", "G:GEN P:LEVEL M:MODE");
}

bool GenrePage::handleEvent(UIEvent& event) {
  if (event.event_type != GROOVEPUTER_KEY_DOWN) return false;
  static UIInput::HoldAccelerator morphAccelerator;

  if (UIInput::isTab(event)) {
    morphAccelerator.reset();
    moveFocus(1);
    return true;
  }
  const int nav = UIInput::navCode(event);
  if (nav == GROOVEPUTER_UP || nav == GROOVEPUTER_DOWN) {
    morphAccelerator.reset();
    moveFocus(nav == GROOVEPUTER_UP ? -1 : 1);
    return true;
  }
  if (nav == GROOVEPUTER_LEFT || nav == GROOVEPUTER_RIGHT) {
    const int delta = nav == GROOVEPUTER_RIGHT ? 1 : -1;
    switch (focus_) {
      case FocusRow::Genre: morphAccelerator.reset(); shiftGenre(delta); return true;
      case FocusRow::Variant:
        morphAccelerator.reset();
        if (event.alt) adjustMorph(delta * 16); else cycleRecipeSelection(delta);
        return true;
      case FocusRow::Rhythm: morphAccelerator.reset(); cycleRhythmSelection(delta); return true;
      case FocusRow::Morph: {
        const bool modified = event.shift || event.ctrl || event.alt || event.meta;
        const int multiplier = modified ? 1 : morphAccelerator.multiplier(delta);
        if (modified) morphAccelerator.reset();
        adjustMorph(delta * (event.shift || event.ctrl ? 32 : 8) * multiplier);
        return true;
      }
      case FocusRow::Apply: morphAccelerator.reset(); cycleApplyMode(delta); return true;
    }
  }

  const char key = static_cast<char>(std::tolower(static_cast<unsigned char>(event.key)));
  const bool keyG = key == 'g' || event.scancode == GROOVEPUTER_G;

  // ENTER follows the APPLY selector. Plain G is always the explicit full
  // Stage 15 materialization command for the pending GENRE/VARIANT/RHYTHM.
  if (event.key == '\n' || event.key == '\r') {
    morphAccelerator.reset();
    applyCurrent();
    return true;
  }
  if (keyG && !event.ctrl && !event.alt && !event.meta) {
    morphAccelerator.reset();
    applyCurrent(true);
    return true;
  }

  if (key == 'p' && !event.ctrl && !event.alt && !event.meta) {
    const auto level = GroovePuterState::cycleGenerationLevel();
    UI::showToast(GroovePuterState::generationLevelShortName(level), 1200);
    return true;
  }

  // Retire the old global I/O generator shortcuts on the GENRE screen. P is
  // owned above by the single P1/P2/P3 generation-request selector.
  if (!event.ctrl && !event.alt && !event.meta &&
      (key == 'i' || key == 'o')) {
    UI::showToast("LEGACY SYNTH GEN OFF", 1200);
    return true;
  }

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
