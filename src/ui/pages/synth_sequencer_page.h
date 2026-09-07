#pragma once

#include "../ui_core.h"
#include "../ui_view_continuity.h"
#include "../phrase_notes_cursor.h"
#include "../phrase_selection_state.h"
#include "../pages/help_dialog.h"
#include "../ui_colors.h"
#include "../ui_utils.h"

class PatternEditPage;
class TB303ParamsPage;

class SynthSequencerPage : public MultiPage, public IMultiHelpFramesProvider {
 public:
  SynthSequencerPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard, int voice_index);

  void draw(IGfx& gfx) override;
  bool handleEvent(UIEvent& ui_event) override;
  const std::string& getTitle() const override;
  void setContext(int context) override;
  void setVisualStyle(VisualStyle style) override;
  void tick() override;

  std::unique_ptr<MultiPageHelpDialog> getHelpDialog() override;
  int getHelpFrameCount() const override;
  void drawHelpFrame(IGfx& gfx, int frameIndex, Rect bounds) const override;

 private:
  enum class SynthTab : uint8_t {
    Notes = 0,
    Knobs,
    More,
  };

  void captureViewContinuity(UI::UiViewContinuityState& state) const override {
    if (voice_index_ < 0 || voice_index_ >= 2) return;
    state.synthTab[voice_index_] = static_cast<uint8_t>(synth_tab_);
    state.phraseCursorCell[voice_index_] = phrase_cursor_.cell;
    state.phraseGrid[voice_index_] = static_cast<uint8_t>(phrase_cursor_.grid);
  }

  void restoreViewContinuity(const UI::UiViewContinuityState& state) override {
    if (voice_index_ < 0 || voice_index_ >= 2) return;
    uint8_t value = state.synthTab[voice_index_];
    if (value > static_cast<uint8_t>(SynthTab::More)) value = 0;
    setSynthTab(static_cast<SynthTab>(value));
    phrase_cursor_.cell = state.phraseCursorCell[voice_index_];
    uint8_t grid = state.phraseGrid[voice_index_];
    if (grid > static_cast<uint8_t>(RuntimePhraseEdit::Grid::ThirtySecond)) {
      grid = static_cast<uint8_t>(RuntimePhraseEdit::Grid::Sixteenth);
    }
    phrase_cursor_.grid = static_cast<RuntimePhraseEdit::Grid>(grid);
  }

  void setSynthTab(SynthTab tab);
  void drawTabIndicator(IGfx& gfx) const;
  const char* activeTabName() const;

  // P3-U1 source-aware NOTES controller. Pattern remains the retained child
  // page; PHRASE gets an independent presentation/controller branch without a
  // second source flag. MiniAcid::SequencedSource is the sole source owner.
  void drawPhraseNotes(IGfx& gfx);
  void drawPhraseRoll(IGfx& gfx);
  void drawPhraseList(IGfx& gfx);
  bool handlePhraseNotesEvent(UIEvent& ui_event);

  MiniAcid& mini_acid_;
  AudioGuard audio_guard_;
  int voice_index_ = 0;
  uint32_t last_tab_switch_ms_ = 0;
  SynthTab synth_tab_ = SynthTab::Notes;
  PhraseNotesCursor::State phrase_cursor_{};
  // The selected sound, shared by both views of the melody. The cursor above
  // keeps the grid and the insert position; this keeps which sound is being
  // edited, so switching view cannot change it.
  PhraseSelectionState::State phrase_selection_{};
  enum class PhraseView : uint8_t { Roll = 0, List };
  PhraseView phrase_view_ = PhraseView::Roll;
  // First visible row of the list. Its own scrolling, as the list and the roll
  // share operations but not presentation.
  uint16_t phrase_list_top_ = 0;
  // Browsing offset for the pitch window only. It is view state, never
  // musical state: it is clamped so the selected sound stays visible and
  // it is not persisted, so looking around can never be mistaken for an
  // edit or survive as one.
  // Lowest visible semitone. The window holds still while the selected
  // sound is inside it and moves only far enough to bring it back when it
  // leaves an edge: recentring on every pick rearranged the whole picture
  // and made the melody hard to follow. 0 means "not established yet".
  int phrase_pitch_lowest_ = 0;
  std::shared_ptr<PatternEditPage> pattern_page_;
  std::shared_ptr<TB303ParamsPage> params_page_;
  std::string fallback_title_;
  std::string phrase_title_;
};
