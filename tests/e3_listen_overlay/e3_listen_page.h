#pragma once

#include <cstdint>
#include <string>

#include "../ui_core.h"
#include "../../generation/migration/e3_listen_fixture_player.h"

class MiniAcid;

class E3ListenPage : public IPage {
 public:
  E3ListenPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard);

  const std::string& getTitle() const override { return title_; }
  void draw(IGfx& gfx) override;
  bool handleEvent(UIEvent& event) override;
  void onEnter(int context) override;
  void onExit() override;

 private:
  struct RuntimeState {
    bool captured = false;
    bool playing = false;
    float bpm = 120.0f;
    bool songMode = false;
    bool loopMode = false;
    int loopStart = 0;
    int loopEnd = 0;
    int activeSongSlot = 0;
    int playbackSongSlot = 0;
    int songPosition = 0;
    int drumBank = 0;
    int drumPattern = 0;
    int synthBank[2] = {0, 0};
    int synthPattern[2] = {0, 0};
    bool muted[10] = {};
  };

  void captureRuntime();
  void restoreRuntime();
  void setTrackMuted(int track, bool muted);
  void setAllTracksMuted(bool muted);
  void applyIsolation();
  void selectCase(int delta);
  void jumpGroup(int delta);
  void setVariant(GroovePuterRhythm::E3ListenVariant variant);
  void replay();
  void togglePlaying();
  bool applySelection();

  MiniAcid& mini_acid_;
  AudioGuard audio_guard_;
  std::string title_ = "E3 LISTEN";
  RuntimeState saved_{};
  uint8_t case_index_ = 0;
  GroovePuterRhythm::E3ListenVariant variant_ =
      GroovePuterRhythm::E3ListenVariant::Canonical;
  bool isolated_ = false;
  bool last_apply_ok_ = false;
};
