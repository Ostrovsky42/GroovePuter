#include <cassert>
#include <cstring>

namespace UI {
void showToast(const char* msg, int durationMs);
}

#define private public
#include "src/dsp/miniacid_engine.h"
#include "src/ui/material_accept_ux.h"
#include "src/ui/material_development_ux.h"
#undef private
#include "src/ui/pages/pattern_edit_page.h"
#include "src/ui/pages/synth_sequencer_page.h"

SerialMock Serial;
SDMock SD;

namespace {

class NullGfx final : public IGfx {
 public:
  void begin() override {}
  void clear(IGfxColor) override {}
  void drawPixel(int, int, IGfxColor) override {}
  void drawText(int, int, const char*) override {}
  void drawImage(int, int, const uint16_t*, int, int) override {}
  void drawRect(int, int, int, int, IGfxColor) override {}
  void drawCircle(int, int, int, IGfxColor) override {}
  void drawKnobFace(int, int, int, IGfxColor, IGfxColor) override {}
  void fillRect(int, int, int, int, IGfxColor) override {}
  void fillCircle(int, int, int, IGfxColor) override {}
  void drawLine(int32_t, int32_t, int32_t, int32_t, IGfxColor) override {}
  void setRotation(int) override {}
  void setTextColor(IGfxColor) override {}
  void setTextColor(uint16_t) override {}
  void setFont(GfxFont) override {}
  void startWrite() override {}
  void endWrite() override {}
  void flush() override {}
  int textWidth(const char* text) const override {
    return text ? static_cast<int>(std::strlen(text)) * 6 : 0;
  }
  int fontHeight() const override { return 8; }
  int width() const override { return 240; }
  int height() const override { return 135; }
};

UIEvent keyDown(char key, bool alt = false, bool ctrl = false, bool meta = false) {
  UIEvent event{};
  event.event_type = GROOVEPUTER_KEY_DOWN;
  event.key = key;
  event.alt = alt;
  event.ctrl = ctrl;
  event.meta = meta;
  return event;
}

void preparePendingMaterial(MiniAcid& engine) {
  engine.pendingMaterial_[0].lifecycleBound = true;
  engine.pendingMaterial_[0].queued = true;
}

void testModifiedEnterIsAcceptNeverGo() {
  MiniAcid engine{44100.0f, nullptr};
  preparePendingMaterial(engine);

  const UIEvent accept = keyDown('\n', true);
  assert(GroovePuterMaterialAcceptUx::isAcceptEvent(accept));
  assert(!GroovePuterMaterialDevelopmentUx::isGoEvent(accept, engine, 0));
}

void testAltBackspaceIsDiscardNeverCancel() {
  MiniAcid engine{44100.0f, nullptr};
  preparePendingMaterial(engine);

  const UIEvent discard = keyDown('\b', true);
  assert(GroovePuterMaterialDevelopmentUx::isDiscardEvent(discard));
  assert(!GroovePuterMaterialDevelopmentUx::isCancelEvent(discard, engine, 0));
}

void testEscapeOnlyDisarmsQueuedGoAndKeepsNext() {
  MiniAcid engine{44100.0f, nullptr};
  preparePendingMaterial(engine);
  engine.goQueued_[0] = true;

  const UIEvent escape = keyDown(0x1B);
  assert(GroovePuterMaterialDevelopmentUx::isCancelEvent(escape, engine, 0));
  assert(GroovePuterMaterialDevelopmentUx::handleCancel(engine, 0));
  assert(engine.hasPendingMaterial(0));
  assert(!engine.isGoQueued(0));
}

template <typename Page>
void verifyPageDispatch(Page& page, MiniAcid& engine) {
  preparePendingMaterial(engine);
  engine.playing = true;

  UIEvent accept = keyDown('\n', true);
  assert(page.handleEvent(accept));
  assert(!engine.isGoQueued(0));
  assert(engine.hasPendingMaterial(0));

  UIEvent discard = keyDown('\b', true);
  assert(page.handleEvent(discard));
  assert(engine.hasPendingMaterial(0));

  engine.goQueued_[0] = true;
  UIEvent escape = keyDown(0x1B);
  assert(page.handleEvent(escape));
  assert(!engine.isGoQueued(0));
  assert(engine.hasPendingMaterial(0));
}

void testPatternEditPageDispatchesMaterialCommands() {
  MiniAcid engine{44100.0f, nullptr};
  NullGfx gfx;
  PatternEditPage page{gfx, engine, AudioGuard{}, 0};
  verifyPageDispatch(page, engine);
}

void testSynthSequencerPageDispatchesMaterialCommands() {
  MiniAcid engine{44100.0f, nullptr};
  NullGfx gfx;
  SynthSequencerPage page{gfx, engine, AudioGuard{}, 0};
  verifyPageDispatch(page, engine);
}

}  // namespace

int main() {
  testModifiedEnterIsAcceptNeverGo();
  testAltBackspaceIsDiscardNeverCancel();
  testEscapeOnlyDisarmsQueuedGoAndKeepsNext();
  testPatternEditPageDispatchesMaterialCommands();
  testSynthSequencerPageDispatchesMaterialCommands();
  return 0;
}
