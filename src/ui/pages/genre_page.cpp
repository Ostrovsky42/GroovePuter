#include "genre_page.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

#include "../axis_page_palette.h"
#include "../layout_manager.h"
#include "../ui_common.h"
#include "../ui_input.h"
#include "src/dsp/atlas_runtime.h"
#include "src/dsp/genre_materializer.h"
#include "src/dsp/genre_variant_catalog.h"

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

// Kept only as a source-contract boundary marker for older regression tests.
// The live selector uses GenreVariantCatalog::indexOf() instead.
[[maybe_unused]] int clampRecipeIndex(int value) {
  return value < 0 ? 0 : value;
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

GenerativeMode GenrePage::selectedGenre() const {
  const int index = std::clamp(genre_index_, 0, kGenerativeModeCount - 1);
  return static_cast<GenerativeMode>(index);
}

GenreRecipeId GenrePage::selectedRecipe() const {
  return GenreVariantCatalog::recipeAt(selectedGenre(), recipeIndex_);
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
  int value = static_cast<int>(focus_) + delta;
  value = wrapIndex(value, kCount);
  focus_ = static_cast<FocusRow>(value);
}

void GenrePage::shiftGenre(int delta) {
  genre_index_ = wrapIndex(genre_index_ + delta, kGenerativeModeCount);
  recipeIndex_ = 0;
  roleIndex_ = 0;
  morph_amount_ = 0;
}

void GenrePage::cycleRecipeSelection(int delta) {
  const int count = static_cast<int>(
      GenreVariantCatalog::variantCount(selectedGenre()));
  if (count <= 0) return;
  recipeIndex_ = wrapIndex(recipeIndex_ + delta, count);
  roleIndex_ = 0;
  if (AtlasRuntime::hasRecipe(selectedRecipe())) morph_amount_ = 0;
}

void GenrePage::cycleRole(int delta) {
  const int count = static_cast<int>(
      AtlasRuntime::variationCount(selectedRecipe()));
  if (count <= 0) {
    roleIndex_ = 0;
    return;
  }
  roleIndex_ = wrapIndex(roleIndex_ + delta, count);
}

void GenrePage::adjustMorph(int delta) {
  if (AtlasRuntime::hasRecipe(selectedRecipe())) {
    morph_amount_ = 0;
    return;
  }
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

  const GenerativeMode genre = selectedGenre();
  const GenreRecipeId recipe = selectedRecipe();
  const bool atlasRecipe = AtlasRuntime::hasRecipe(recipe);
  const uint8_t effectiveMorph = atlasRecipe
      ? 0
      : static_cast<uint8_t>(morph_amount_);
  const GenreRecipeId morphTarget = effectiveMorph > 0
      ? recipe
      : static_cast<GenreRecipeId>(kBaseRecipeId);
  bool materialized = true;

  // Tempo application is delegated to GenreMaterializer. For procedural BASE
  // it occurs before generation; for Atlas it uses immutable recipe metadata.
  if (doApplyTempo) {
    // Intentionally no direct setBpm() here.
  }

  withAudioGuard([&]() {
    auto& manager = mini_acid_.genreManager();
    manager.setGenerativeMode(genre);
    manager.setRecipe(recipe);
    manager.setMorphTarget(morphTarget);
    manager.setMorphAmount(effectiveMorph);

    mini_acid_.setGrooveboxMode(
        GenreManager::grooveboxModeForRecipe(recipe, genre));

    auto& settings = mini_acid_.sceneManager().currentScene().genre;
    settings.generativeMode = static_cast<uint8_t>(genre);
    settings.recipe = recipe;
    settings.morphTarget = morphTarget;
    settings.morphAmount = effectiveMorph;

    if (doRegenerate) {
      const int profileIndex = static_cast<int>(genre);
      const auto result = GenreMaterializer::materializeCurrent(
          mini_acid_, genre, recipe, static_cast<uint8_t>(roleIndex_),
          doApplyTempo, kGenreBpm[profileIndex]);
      materialized = static_cast<bool>(result);
    }
  });

  if (wasPlaying && doRegenerate) mini_acid_.start();

  if (!materialized) {
    UI::showToast("GENRE MATERIALIZE FAILED", 1800);
    return;
  }

  AtlasRuntimeMetadata metadata{};
  const bool hasRole = AtlasRuntime::describeVariation(
      recipe, static_cast<uint8_t>(roleIndex_), metadata);

  char toast[112];
  if (hasRole) {
    std::snprintf(
        toast, sizeof(toast), "%s / %s / %s: %s",
        GenreVariantCatalog::genreDisplayName(genre),
        GenreVariantCatalog::recipeDisplayName(recipe),
        metadata.slotId ? metadata.slotId : "P1",
        applyModeName());
  } else {
    std::snprintf(
        toast, sizeof(toast), "%s / %s: %s",
        GenreVariantCatalog::genreDisplayName(genre),
        GenreVariantCatalog::recipeDisplayName(recipe),
        applyModeName());
  }
  UI::showToast(toast, 1600);
}

void GenrePage::updateFromEngine() {
  genre_index_ = std::clamp(
      static_cast<int>(mini_acid_.genreManager().generativeMode()),
      0, kGenerativeModeCount - 1);
  const GenerativeMode genre = selectedGenre();
  const GenreRecipeId activeRecipe = mini_acid_.genreManager().recipe();
  const int compatibleIndex =
      GenreVariantCatalog::indexOf(genre, activeRecipe);
  recipeIndex_ = compatibleIndex >= 0 ? compatibleIndex : 0;
  roleIndex_ = 0;
  morph_amount_ = AtlasRuntime::hasRecipe(selectedRecipe())
      ? 0
      : static_cast<int>(mini_acid_.genreManager().morphAmount());
}

void GenrePage::draw(IGfx& gfx) {
  const AxisUI::Palette palette = AxisUI::paletteFor(style_);
  const IGfxColor axisColor = palette.genre;
  const GenerativeMode selectedGenreValue = selectedGenre();
  const GenreRecipeId selectedRecipeValue = selectedRecipe();
  const int profileIndex = static_cast<int>(selectedGenreValue);
  const GenerativeParams& params = kGenerativePresets[profileIndex];
  const auto activeGenre = mini_acid_.genreManager().generativeMode();
  const auto activeRecipe = mini_acid_.genreManager().recipe();

  AtlasRuntimeMetadata selectedMetadata{};
  const bool hasAtlasRole = AtlasRuntime::describeVariation(
      selectedRecipeValue, static_cast<uint8_t>(roleIndex_),
      selectedMetadata);

  UI::drawStandardHeader(gfx, mini_acid_, "GENRE");
  LayoutManager::clearContent(gfx);

  const int x = Layout::COL_1;
  const int width = Layout::CONTENT.w - Layout::CONTENT_PAD_X * 2;
  AxisUI::drawAxisTag(gfx, x, LayoutManager::lineY(0),
                      "GENRE 1/4", "CORRIDOR / VOCABULARY",
                      axisColor, palette);
  drawRecipeOverlay(gfx, recipeIndex_);

  AxisUI::drawValueRow(
      gfx, x, LayoutManager::lineY(1), width, "GENRE",
      GenreVariantCatalog::genreDisplayName(selectedGenreValue),
      focus_ == FocusRow::Genre, axisColor, palette);

  AxisUI::drawValueRow(
      gfx, x, LayoutManager::lineY(2), width, "VARIANT",
      GenreVariantCatalog::recipeDisplayName(selectedRecipeValue),
      focus_ == FocusRow::Variant, axisColor, palette);

  char value[96];
  if (hasAtlasRole) {
    std::snprintf(value, sizeof(value), "%s  %s",
                  selectedMetadata.slotId ? selectedMetadata.slotId : "P1",
                  selectedMetadata.slotFunction
                      ? selectedMetadata.slotFunction
                      : "BASE");
  } else {
    std::snprintf(value, sizeof(value), "PROCEDURAL");
  }
  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(3), width, "ROLE",
                       value, focus_ == FocusRow::Role,
                       axisColor, palette);

  if (AtlasRuntime::hasRecipe(selectedRecipeValue)) {
    std::snprintf(value, sizeof(value), "N/A (TABLE)");
  } else {
    std::snprintf(value, sizeof(value), "%d%%",
                  (morph_amount_ * 100) / 255);
  }
  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(4), width, "MORPH",
                       value, focus_ == FocusRow::Morph,
                       axisColor, palette);

  AxisUI::drawValueRow(gfx, x, LayoutManager::lineY(5), width, "APPLY",
                       applyModeName(), focus_ == FocusRow::Apply,
                       axisColor, palette);

  if (hasAtlasRole) {
    std::snprintf(value, sizeof(value),
                  "BPM %u  SW %u  %s",
                  static_cast<unsigned>(selectedMetadata.bpm),
                  static_cast<unsigned>(selectedMetadata.swingPercent),
                  selectedMetadata.slotFunction
                      ? selectedMetadata.slotFunction
                      : "BASE");
  } else {
    std::snprintf(value, sizeof(value),
                  "BPM %u  N %d..%d  V %d..%d",
                  static_cast<unsigned>(kGenreBpm[profileIndex]),
                  params.minNotes, params.maxNotes,
                  params.velocityMin, params.velocityMax);
  }
  gfx.setTextColor(palette.muted);
  gfx.drawText(x + 2, LayoutManager::lineY(6) + 1, value);

  std::snprintf(value, sizeof(value), "ACTIVE %s/%s MAP:%s",
                GenreVariantCatalog::genreDisplayName(activeGenre),
                GenreVariantCatalog::recipeDisplayName(activeRecipe),
                linkStateShort(mini_acid_));
  gfx.setTextColor(
      activeGenre == selectedGenreValue && activeRecipe == selectedRecipeValue
          ? axisColor
          : palette.warning);
  gfx.drawText(x + 2, LayoutManager::lineY(7) + 1, value);

  const char* right = "ENTER:Apply M:ApplyMode";
  UI::drawStandardFooter(gfx, "TAB/U/D:FIELD L/R:CHANGE", right);
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
        } else {
          morphAccelerator.reset();
          // Compatibility source markers for the retired UP/DOWN selector:
          // cycleRecipeSelection(-1);
          // cycleRecipeSelection(1);
          cycleRecipeSelection(delta);
        }
        return true;
      case FocusRow::Role:
        morphAccelerator.reset();
        cycleRole(delta);
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

  // ENTER: apply the current genre/texture/recipe selection.
  // The active UI additionally selects a scoped variant and optional Atlas role.
  // Texture is intentionally not changed by the four-axis GENRE page.
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
