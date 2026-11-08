#pragma once

#include "../ui_core.h"
#include "../pages/help_dialog.h"
#include "../ui_colors.h"
#include "../ui_utils.h"
#include "src/generation/rhythm_browser/rhythm_browser_catalog.h"

class BankSelectionBarComponent;
class PatternSelectionBarComponent;

// Keeps the previous MultiPage event path available after the source-level
// handler is renamed to handleEventLegacy by the input-lock wrapper.
class DrumSequencerLegacyMultiPage : public MultiPage {
 public:
  bool handleEventLegacy(UIEvent& ui_event) {
    return MultiPage::handleEvent(ui_event);
  }
};

class DrumSequencerPage : public DrumSequencerLegacyMultiPage,
                          public IMultiHelpFramesProvider {
 public:
  DrumSequencerPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard);
  bool handleEvent(UIEvent& ui_event) override;
  void draw(IGfx& gfx) override;
  const std::string & getTitle() const override;
  std::unique_ptr<MultiPageHelpDialog> getHelpDialog() override;
  int getHelpFrameCount() const override;
  void drawHelpFrame(IGfx& gfx, int frameIndex, Rect bounds) const override;
  void setContext(int context) override; // context: (voice << 8) | step
  void onExit() override;
  void tick() override;

 private:
  struct BrowserBuffers {
    RhythmBrowser::Workspace workspace{};
    RhythmBrowser::Candidate candidate{};
    // Miniature of the selected rhythm: per drum voice, a bit per step. soft
    // marks hits below the ghost velocity. Keyed by what was materialized, so
    // it is rebuilt only when the selection, variant or scene revision changes.
    struct Miniature {
      uint16_t hit[8] = {};
      uint16_t soft[8] = {};
      uint8_t index = 0;
      uint8_t variant = 0;
      uint32_t revision = 0;
      bool valid = false;
    } miniature;
  };
  std::unique_ptr<BrowserBuffers> browserBuffers_;
  bool browserOpen_ = false;
  bool browserStartedTransport_ = false;
  bool browserPending_ = false;
  bool browserSolo_ = false;
  uint8_t browserIndex_ = 0;
  uint8_t browserVariant_ = 0;
  uint8_t browserGroup_ = 0;
  uint32_t browserSession_ = 0;
  // Audition-only mutes reuse the engine track mutes (1..0, isTrackActive
  // index). The state before HEAR is kept here and restored when the audition
  // ends (Tab, Enter, Esc, leaving DRUMS, transport stop).
  bool browserMuteSnapshotValid_ = false;
  bool browserMuteSnapshot_[10] = {};
  void snapshotBrowserMutes_();
  void restoreBrowserMutes_();
  bool toggleBrowserMute_(char key);
  void stopBrowserPreview_();
  void closeBrowser_();
  void drawBrowser_(IGfx& gfx);
  bool handleBrowserEvent_(UIEvent& event);
  void openRhythmBrowser_();
  bool materializeBrowserCandidate_();
  void refreshBrowserMiniature_();
  bool stageBrowserCandidate_();
  bool applyBrowserCandidate_();
  void selectBrowserStep_(int direction, bool keepCurrent = false);
  bool browserSelectionInGroup_() const;
  bool handleEventLegacy(UIEvent& ui_event);
};
