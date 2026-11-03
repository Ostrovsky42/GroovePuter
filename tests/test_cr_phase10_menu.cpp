// CALL/RESPONSE phase 10: PERFORM > INPUT no longer shows the PARTNER GENRE
// and LENGTH rows. The listener keeps AUTO (project genre) and 100 %; the
// setters stay (test_cr_partner_settings.cpp, test_cr_phase9_length.cpp).
// New layout: 8 PARTNER, 9 BARS, 10 CHANNEL, 11 HOLD, 12 EVOLVE, 13 REPLACE.
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "src/input/call_response_listener.h"
#include "src/input/musical_event_router.h"
#include "src/input/performance_keyboard.h"
#include "src/ui/pages/perform_page.h"
#include "src/ui/ui_common.h"
#include "src/ui/ui_view_continuity.h"

SerialMock Serial;
SDMock SD;

namespace {
using Listener = GroovePuterCallResponse::CallResponseListener;

class TextGfx : public IGfx {
 public:
  void begin() override {}
  void clear(IGfxColor) override {}
  void drawPixel(int, int, IGfxColor) override {}
  void drawText(int, int, const char* t) override { if (t) texts.emplace_back(t); }
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
  int textWidth(const char* t) const override { return t ? static_cast<int>(std::strlen(t)) * 6 : 0; }
  int fontHeight() const override { return 8; }
  int width() const override { return 240; }
  int height() const override { return 135; }
  bool saw(const char* needle) const {
    for (const auto& t : texts) if (t.find(needle) != std::string::npos) return true;
    return false;
  }
  bool sawExact(const char* text) const {
    for (const auto& t : texts) if (t == text) return true;
    return false;
  }
  std::vector<std::string> texts;
};

UIEvent key(char c) {
  UIEvent e{};
  e.event_type = GROOVEPUTER_KEY_DOWN;
  e.key = c;
  return e;
}

UIEvent nav(KeyScanCode code) {
  UIEvent e{};
  e.event_type = GROOVEPUTER_KEY_DOWN;
  e.scancode = code;
  return e;
}

void send(IPage& page, UIEvent e) { assert(page.handleEvent(e)); }

UI::UiViewContinuityState capture(const IPage& page) {
  UI::UiViewContinuityState s{};
  page.captureViewContinuity(s);
  return s;
}

uint8_t inputRow(const IPage& page) {
  const auto s = capture(page);
  assert(s.performContext == static_cast<uint8_t>(PerformanceToolContext::Input));
  return s.performRows[static_cast<int>(PerformanceToolContext::Input)];
}

void assertDefaults(const Listener& partner) {
  assert(partner.answerGenreSetting() == Listener::kGenreAuto);
  assert(partner.answerLengthPercent() == 100);
}

void goToRow(IPage& page, uint8_t row) {
  for (int guard = 0; inputRow(page) != row; ++guard) {
    assert(guard < 32);
    send(page, nav(GROOVEPUTER_DOWN));
  }
}
}  // namespace

int main() {
  MiniAcid engine(44100.0f, nullptr);
  MusicalEventRouter router;
  PerformanceKeyboard keyboard(router);
  TextGfx gfx;
  PerformPage page(gfx, engine, keyboard);
  IPage& base = page;
  page.onEnter(0);
  auto& partner = GroovePuterCallResponse::callResponseListener();
  assertDefaults(partner);

  // Tab opens the tools layer on KEY; four more reach INPUT.
  for (int i = 0; i < 5; ++i) send(base, key('\t'));
  assert(inputRow(base) == 0);
  assertDefaults(partner);  // entering INPUT changes nothing

  // DOWN visits exactly rows 0..13 and wraps; UP wraps back to 13.
  for (uint8_t expected = 1; expected <= 13; ++expected) {
    send(base, nav(GROOVEPUTER_DOWN));
    assert(inputRow(base) == expected);
  }
  send(base, nav(GROOVEPUTER_DOWN));
  assert(inputRow(base) == 0);
  send(base, nav(GROOVEPUTER_UP));
  assert(inputRow(base) == 13);

  // Every frame of the INPUT list (all scroll positions): no GENRE or LENGTH
  // label, BARS / CHANNEL / HOLD / EVOLVE / REPLACE still drawn.
  bool sawBars = false, sawChannel = false, sawHold = false, sawEvolve = false, sawReplace = false;
  for (uint8_t row = 0; row < 14; ++row) {
    goToRow(base, row);
    gfx.texts.clear();
    page.drawContent(gfx);  // the tools layer; the header may name the project genre
    // Row labels are drawn alone (" GENRE" / " LENGTH"); the BARS hint
    // legitimately says "REPLY LENGTH", so labels are matched exactly.
    assert(!gfx.sawExact(" GENRE") && !gfx.sawExact(" LENGTH"));
    assert(!gfx.saw("AUTO=PROJECT") && !gfx.saw("ANSWER LENGTH: 100"));
    sawBars |= gfx.sawExact(" BARS");
    sawChannel |= gfx.sawExact(" CHANNEL");
    sawHold |= gfx.sawExact(" HOLD");
    sawEvolve |= gfx.sawExact(" EVOLVE");
    sawReplace |= gfx.sawExact(" REPLACE");
  }
  assert(sawBars && sawChannel && sawHold && sawEvolve && sawReplace);

  // The renumbered rows act on what they name; </> and ENTER on any PARTNER
  // row never touch GENRE or LENGTH.
  partner.setMode(Listener::Mode::Loop);
  goToRow(base, 9);
  const uint8_t bars = partner.answerBars();
  send(base, nav(GROOVEPUTER_RIGHT));
  assert(partner.answerBars() != bars);
  send(base, nav(GROOVEPUTER_LEFT));
  assert(partner.answerBars() == bars);
  assertDefaults(partner);

  goToRow(base, 10);
  const uint8_t channel = partner.answerChannel();
  send(base, nav(GROOVEPUTER_RIGHT));
  assert(partner.answerChannel() != channel);
  send(base, nav(GROOVEPUTER_LEFT));
  assert(partner.answerChannel() == channel);
  assertDefaults(partner);

  goToRow(base, 12);
  const uint8_t evolve = partner.loopEvolve();
  send(base, nav(GROOVEPUTER_RIGHT));
  assert(partner.loopEvolve() != evolve);
  send(base, nav(GROOVEPUTER_LEFT));
  assert(partner.loopEvolve() == evolve);
  assertDefaults(partner);

  goToRow(base, 13);
  const auto replace = partner.loopReplace();
  send(base, nav(GROOVEPUTER_RIGHT));
  assert(partner.loopReplace() != replace);
  send(base, nav(GROOVEPUTER_LEFT));
  assert(partner.loopReplace() == replace);
  assertDefaults(partner);

  for (uint8_t row = 9; row <= 13; ++row) {
    goToRow(base, row);
    send(base, nav(GROOVEPUTER_RIGHT));
    send(base, nav(GROOVEPUTER_LEFT));
    send(base, key('\r'));
    assertDefaults(partner);
  }
  partner.setMode(Listener::Mode::Off);

  // View continuity: the last row (13) survives a page round trip; a stale
  // row from the old 16-row list (14, 15) falls back to the top.
  goToRow(base, 13);
  UI::UiViewContinuityState saved = capture(base);
  PerformPage other(gfx, engine, keyboard);
  IPage& otherBase = other;
  otherBase.restoreViewContinuity(saved);
  assert(inputRow(otherBase) == 13);
  for (uint8_t stale = 14; stale <= 15; ++stale) {
    saved.performRows[static_cast<int>(PerformanceToolContext::Input)] = stale;
    otherBase.restoreViewContinuity(saved);
    assert(inputRow(otherBase) == 0);
  }
  assertDefaults(partner);

  std::puts("cr phase10 menu tests passed");
  return 0;
}
