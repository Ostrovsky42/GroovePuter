#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include "../ui_core.h"
#include "src/dsp/miniacid_engine.h"
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
    Morph,
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
  void shiftVariant(int delta);
  void adjustMorph(int delta);
  void cycleApplyMode(int delta);
  void applyCurrent();

  ApplyMode currentApplyMode() const;
  const char* applyModeName() const;

  template <typename F>
  void withAudioGuard(F&& fn) {
    if (audio_guard_) audio_guard_(std::forward<F>(fn));
    else fn();
    GroovePuterState::markSceneMutated();
  }

  MiniAcid& mini_acid_;
  AudioGuard audio_guard_;
  VisualStyle style_ = VisualStyle::MINIMAL;
  FocusRow focus_ = FocusRow::Genre;
  int genre_index_ = 0;
  int recipe_index_ = 0;
  int morph_amount_ = 0;
  std::string title_ = "GENRE";
};
