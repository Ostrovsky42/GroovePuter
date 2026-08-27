#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include "../ui_core.h"
#include "src/dsp/miniacid_engine.h"
#include "src/generation/composition/rhythm_selection.h"
#include "src/state/scene_revision.h"

class GenrePage : public IPage {
 public:
  GenrePage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard);

  void draw(IGfx& gfx) override;
  bool handleEvent(UIEvent& ui_event) override;
  const std::string& getTitle() const override { return title_; }
  void setVisualStyle(VisualStyle style) override { style_ = style; }

 private:
  enum class FocusRow : uint8_t {
    Genre = 0,
    Variant,
    Rhythm,
    Depth,
    Apply,
  };

  enum class ApplyMode : uint8_t {
    ProfileOnly = 0,
    Regenerate,
    RegenerateTempo,
  };

  void updateFromEngine();
  void moveFocus(int delta);
  void shiftGenre(int delta);
  void cycleRecipeSelection(int delta);
  void cycleRhythmSelection(int delta);
  bool normalizePendingRhythm(bool notify);
  void cycleApplyMode(int delta);
  void applyCurrent(bool forceRegenerate = false);

  ApplyMode currentApplyMode() const;
  const char* applyModeName() const;
  GenreSettings pendingSettings() const;

  template <typename F>
  void withAudioGuard(F&& fn) {
    if (audio_guard_) audio_guard_(std::forward<F>(fn));
    else fn();
  }

  MiniAcid& mini_acid_;
  AudioGuard audio_guard_;
  VisualStyle style_ = VisualStyle::MINIMAL;
  FocusRow focus_ = FocusRow::Genre;
  int genre_index_ = 0;
  int recipeIndex_ = 0;
  GroovePuterRhythm::RhythmSelectionMode rhythmMode_ =
      GroovePuterRhythm::RhythmSelectionMode::Auto;
  GroovePuterRhythm::RhythmArchetypeId rhythmArchetypeId_ =
      GroovePuterRhythm::kNoArchetypeId;
  bool rhythmFallbackPending_ = false;
  std::string title_ = "GENRE";
};
