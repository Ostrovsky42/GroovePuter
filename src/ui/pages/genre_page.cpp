#include "genre_page.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

#include "../axis_page_palette.h"
#include "../layout_manager.h"
#include "../ui_common.h"
#include "../ui_input.h"
#include "../../generation/composition/generation_profile.h"
#include "../../generation/migration/quantized_generation_commit.h"
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
  settings.morphTarget = 0;
  settings.morphAmount = 0;
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

  const auto genre = static_cast<GenerativeMode>(genre_index_);
  const auto recipe = normalizeRecipeForGenre(
      genre, static_cast<GenreRecipeId>(recipeIndex_));
  recipeIndex_ = static_cast<int>(recipe);
  normalizePendingRhythm(true);
  const GrooveboxMode nextMode = GenreCatalog::grooveboxModeForRecipe(recipe, genre);
  auto& activeSettings = mini_acid_.sceneManager().currentScene().genre;

  GenreSettings requestedSettings = activeSettings;
  requestedSettings.generativeMode = static_cast<uint8_t>(genre_index_);
  requestedSettings.recipe = static_cast<uint8_t>(recipe);
  requestedSettings.morphTarget = 0;
  requestedSettings.morphAmount = 0;
  requestedSettings.rhythmSelectionMode = static_cast<uint8_t>(rhythmMode_);
  requestedSettings.rhythmArchetypeId = rhythmArchetypeId_;

  const bool changed = doRegenerate || mini_acid_.grooveboxMode() != nextMode ||
      activeSettings.generativeMode != requestedSettings.generativeMode ||
      activeSettings.recipe != requestedSettings.recipe ||
      activeSettings.morphTarget != requestedSettings.morphTarget ||
      activeSettings.morphAmount != requestedSettings.morphAmount ||
      activeSettings.rhythmSelectionMode != requestedSettings.rhythmSelectionMode ||
      activeSettings.rhythmArchetypeId != requestedSettings.rhythmArchetypeId;

  float requestedBpm = mini_acid_.bpm();
  if (doApplyTempo) {
    const GroovePuterRhythm::GenerationProfileView profile =
        GroovePuterRhythm::generationProfileFor(requestedSettings);
    if (profile.corridor.suggestedBpm > 0)
      requestedBpm = static_cast<float>(profile.corridor.suggestedBpm);
  }

  GroovePuterRhythm::QuantizedGenerationResult generationResult =
      GroovePuterRhythm::QuantizedGenerationResult::Failed;

  if (doRegenerate && mini_acid_.isPlaying()) {
    // PLAY preparation is scratch-only and deliberately does not acquire the
    // AudioMutationGate. AudioTask keeps rendering the current bar while the
    // complete next-bar candidate is built and atomically published.
    generationResult = GroovePuterRhythm::regenerateWithQuantizedCommit(
        mini_acid_, requestedSettings, nextMode, doApplyTempo, requestedBpm);
  } else {
    withAudioGuard([&]() {
      if (doRegenerate) {
        generationResult = GroovePuterRhythm::regenerateWithQuantizedCommit(
            mini_acid_, requestedSettings, nextMode, doApplyTempo, requestedBpm);
        return;
      }
      activeSettings = requestedSettings;
      mini_acid_.setGrooveboxMode(nextMode);
    });
  }

  if (changed && !doRegenerate) GroovePuterState::markSceneMutated();

  const char* resultLabel = applyModeName();
  if (doRegenerate) {
    switch (generationResult) {
      case GroovePuterRhythm::QuantizedGenerationResult::PendingNextBar:
        resultLabel = "GEN -> NEXT BAR";
        break;
      case GroovePuterRhythm::QuantizedGenerationResult::CommittedNow:
        resultLabel = "GENERATED";
        break;
      case GroovePuterRhythm::QuantizedGenerationResult::AttemptUnavailable:
        resultLabel = "GEN ATTEMPT FULL";
        break;
      case GroovePuterRhythm::QuantizedGenerationResult::Failed:
      default:
        resultLabel = "GEN FAILED";
        break;
    }
  }

  char toast[96];
  std::snprintf(toast, sizeof(toast), "%s / %s: %s",
                GenreCatalog::generativeModeName(genre),
                GenreCatalog::recipeName(recipe), resultLabel);
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
  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(2), width, "RECIPE", value,
                       focus_ == FocusRow::Variant, axisColor, palette);

  const char* rhythmName = "AUTO";
  if (rhythmMode_ == GroovePuterRhythm::RhythmSelectionMode::Manual) {
    const char* selectedName = GroovePuterRhythm::rhythmSelectionName(rhythmArchetypeId_);
    rhythmName = selectedName == nullptr ? "AUTO" : selectedName;
  }
  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(3), width, "RHYTHM", rhythmName,
                       focus_ == FocusRow::Rhythm, axisColor, palette);

  AxisUI::drawValueRow(
      gfx, x, LayoutManager::lineY(4), width, "DEPTH",
      GroovePuterState::generationLevelShortName(
          GroovePuterState::currentGenerationLevel()),
      focus_ == FocusRow::Depth, axisColor, palette);

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

  const char* pendingSuffix =
      GroovePuterRhythm::hasPendingQuantizedGeneration(mini_acid_)
          ? " NEXT"
          : "";
  std::snprintf(value, sizeof(value), "A:%s/%s %s %s%s",
      GenreCatalog::generativeModeName(activeGenre),
      GenreCatalog::recipeName(activeRecipe),
      linkStateShort(mini_acid_),
      GroovePuterState::generationLevelShortName(
          GroovePuterState::currentGenerationLevel()),
      pendingSuffix);
  gfx.setTextColor(activeGenre == selectedGenre && activeRecipe == selectedRecipe
                       ? axisColor : palette.warning);
  gfx.drawText(x + 2, LayoutManager::lineY(7) + 1, value);

  UI::drawStandardFooter(gfx, "TAB/U/D:FIELD L/R:CHANGE", "G:GEN P:DEPTH M:APPLY");
}

bool GenrePage::handleEvent(UIEvent& event) {
  if (event.event_type == GROOVEPUTER_APPLICATION_EVENT &&
      event.app_event_type == GROOVEPUTER_APP_EVENT_UNDO) {
    auto& owner = GroovePuterUndo::undoOwner();
    if (!owner.hasUndo() ||
        owner.kind() != GroovePuterUndo::UndoKind::Generation) {
      return false;
    }
    const bool redo = owner.nextIsRedo();
    GroovePuterUndo::UndoResult result = GroovePuterUndo::UndoResult::KindMismatch;
    withAudioGuard([&]() {
      result = GroovePuterRhythm::toggleLastQuantizedGeneration(mini_acid_);
    });
    if (result == GroovePuterUndo::UndoResult::Restored) {
      updateFromEngine();
      UI::showToast(redo ? "REDO: GEN" : "UNDO: GEN", 1000);
      return true;
    }
    if (result == GroovePuterUndo::UndoResult::ContextUnavailable) {
      UI::showToast(redo ? "REDO: STOP OR WAIT" : "UNDO: STOP OR WAIT", 1100);
      return true;
    }
    if (result == GroovePuterUndo::UndoResult::Expired) {
      UI::showToast("UNDO: EXPIRED", 900);
      return true;
    }
    return false;
  }
  if (event.event_type != GROOVEPUTER_KEY_DOWN) return false;

  if (UIInput::isTab(event)) {
    moveFocus(1);
    return true;
  }
  const int nav = UIInput::navCode(event);
  if (nav == GROOVEPUTER_UP || nav == GROOVEPUTER_DOWN) {
    moveFocus(nav == GROOVEPUTER_UP ? -1 : 1);
    return true;
  }
  if (nav == GROOVEPUTER_LEFT || nav == GROOVEPUTER_RIGHT) {
    const int delta = nav == GROOVEPUTER_RIGHT ? 1 : -1;
    switch (focus_) {
      case FocusRow::Genre: shiftGenre(delta); return true;
      case FocusRow::Variant: cycleRecipeSelection(delta); return true;
      case FocusRow::Rhythm: cycleRhythmSelection(delta); return true;
      case FocusRow::Depth:
        (void)GroovePuterState::cycleGenerationLevel(delta);
        return true;
      case FocusRow::Apply: cycleApplyMode(delta); return true;
    }
  }

  const char key = static_cast<char>(std::tolower(static_cast<unsigned char>(event.key)));
  const bool keyG = key == 'g' || event.scancode == GROOVEPUTER_G;
  const bool keyP = key == 'p' || event.scancode == GROOVEPUTER_P;

  // ENTER follows the APPLY selector. Plain G is always the explicit full
  // Stage 15 materialization command. Repeated accepted G requests advance the
  // bounded session reroll ordinal for this mode/recipe/P-level/address tuple.
  if (event.key == '\n' || event.key == '\r') {
    applyCurrent();
    return true;
  }
  if (keyG && !event.ctrl && !event.alt && !event.meta) {
    applyCurrent(true);
    return true;
  }

  if (keyP && !event.ctrl && !event.alt && !event.meta) {
    const auto level = GroovePuterState::cycleGenerationLevel();
    UI::showToast(GroovePuterState::generationLevelShortName(level), 1200);
    return true;
  }

  if (!event.ctrl && !event.alt && !event.meta &&
      (key == 'i' || key == 'o')) {
    UI::showToast("LEGACY SYNTH GEN OFF", 1200);
    return true;
  }

  if (event.key == ' ' && focus_ == FocusRow::Apply) {
    cycleApplyMode(1);
    return true;
  }
  if (key == 'm' && !event.ctrl && !event.alt && !event.meta) {
    cycleApplyMode(1);
    return true;
  }
  return false;
}
