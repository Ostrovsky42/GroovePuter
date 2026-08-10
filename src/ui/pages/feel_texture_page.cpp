#include "feel_texture_page.h"

FeelTexturePage::FeelTexturePage(IGfx& gfx,
                                 MiniAcid& mini_acid,
                                 AudioGuard audio_guard)
    : audio_guard_(std::move(audio_guard)),
      feel_page_(gfx, mini_acid, audio_guard_) {}

void FeelTexturePage::draw(IGfx& gfx) {
  feel_page_.draw(gfx);
}

bool FeelTexturePage::handleEvent(UIEvent& ui_event) {
  return feel_page_.handleEvent(ui_event);
}

const std::string& FeelTexturePage::getTitle() const {
  return feel_page_.getTitle();
}

void FeelTexturePage::setVisualStyle(VisualStyle style) {
  feel_page_.setVisualStyle(style);
}
