// CALL/RESPONSE phase 10: PERFORM > INPUT no longer shows the PARTNER GENRE
// and LENGTH rows. The listener keeps AUTO (project genre) and 100 %; the
// setters stay (test_cr_partner_settings.cpp, test_cr_phase9_length.cpp).
// Layout (0.9.23 T14): the [INPUT] tab (PerformInputRow) has two groups under
// headings that never take the cursor. --- MIDI IN ---: MIDI IN, LISTEN CH,
// SEND TO, PLAY AS, LEAD STYLE. --- HELPER ---: HELPER (OFF/ANSWER/DUET/RIFF),
// REPLY BARS, REPLY CH, FREEZE RIFF, VARIATION, NEW RIFF IF. The [LOOP] tab
// (PerformLoopRow, nine rows, six visible, scrolls): LOOPER, REC, LOOP BARS,
// LOOP CH (value DX ROUTE), NOTE LENGTH, HOLD LAYER, LAYER, MUTE LAYER,
// TO MELODY.
// TARGET, OUT and USB are not INPUT rows any more (KEY / PROJECT > MIDI).
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "src/input/call_response_listener.h"
#include "src/input/looper.h"
#include "src/input/musical_event_router.h"
#include "src/input/performance_keyboard.h"
#include "src/state/generation_shape_state.h"
#include "src/state/undo_owner.h"
#include "src/ui/components/step_timing_mark.h"
#include "src/ui/pages/perform_page.h"
#include "src/ui/ui_common.h"
#include "src/ui/ui_view_continuity.h"

SerialMock Serial;
SDMock SD;

namespace {
using Listener = GroovePuterCallResponse::CallResponseListener;
using GroovePuterLooper::Looper;
constexpr uint8_t kLast = PerformPage::kPartnerRowLast;
constexpr uint8_t row(PerformInputRow r) { return performInputRow(r); }
using R = PerformInputRow;
using L = PerformLoopRow;

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
  void fillRect(int x, int y, int w, int h, IGfxColor) override { rects.push_back({x, y, w, h}); }
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
  struct Rect4 { int x, y, w, h; };
  std::vector<Rect4> rects;
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

uint8_t contextOf(const IPage& page) { return capture(page).performContext; }

uint8_t inputRow(const IPage& page) {
  const auto s = capture(page);
  assert(s.performContext == static_cast<uint8_t>(PerformanceToolContext::Input));
  return s.performRows[static_cast<int>(PerformanceToolContext::Input)];
}

uint8_t loopRow(const IPage& page) {
  const auto s = capture(page);
  assert(s.performContext == static_cast<uint8_t>(PerformanceToolContext::Loop));
  return s.performRows[static_cast<int>(PerformanceToolContext::Loop)];
}

void assertDefaults(const Listener& partner) {
  assert(partner.answerGenreSetting() == Listener::kGenreAuto);
  assert(partner.answerLengthPercent() == 100);
}

void goToContext(IPage& page, PerformanceToolContext target) {
  for (int guard = 0; contextOf(page) != static_cast<uint8_t>(target); ++guard) {
    assert(guard < 8);
    send(page, key('\t'));
  }
}

void goInput(IPage& page, uint8_t row) {
  goToContext(page, PerformanceToolContext::Input);
  for (int guard = 0; inputRow(page) != row; ++guard) {
    assert(guard < 32);
    send(page, nav(GROOVEPUTER_DOWN));
  }
}

void goLoop(IPage& page, PerformLoopRow row) {
  goToContext(page, PerformanceToolContext::Loop);
  for (int guard = 0; loopRow(page) != performLoopRow(row); ++guard) {
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

  // DOWN visits exactly the 11 rows 0..10 (headings are not rows) and wraps;
  // UP wraps back to 10. (Before the LOOP SETUP move: 14 rows, HELPER 8..13.)
  static_assert(PerformPage::kPartnerRowFirst == 5 && kLast == 10,
                "MIDI IN 0..4, PARTNER 5..10");
  static_assert(PerformPage::kLoopRowCount == 9, "the LOOP tab has nine rows");
  static_assert(PerformPage::kInputRowCount == kLast + 1, "NEW RIFF IF is the last row");
  for (uint8_t expected = 1; expected <= kLast; ++expected) {
    send(base, nav(GROOVEPUTER_DOWN));
    assert(inputRow(base) == expected);
  }
  send(base, nav(GROOVEPUTER_DOWN));
  assert(inputRow(base) == 0);
  send(base, nav(GROOVEPUTER_UP));
  assert(inputRow(base) == kLast);

  // Every frame of the INPUT list (all scroll positions): no GENRE or LENGTH
  // label, every new label and the three group headings drawn; the old labels
  // (TARGET, OUT, USB, LIVE MIDI, LP ...) and the PARTNER mode word LOOP gone.
  const char* const kLabels[] = {
      "MIDI IN", "LISTEN CH", "SEND TO", "PLAY AS", "LEAD STYLE",
      "HELPER", "REPLY BARS", "REPLY CH", "FREEZE RIFF", "VARIATION", "NEW RIFF IF",
      "--- MIDI IN ---", "--- HELPER ---",
  };
  constexpr int kLabelCount = sizeof(kLabels) / sizeof(kLabels[0]);
  bool sawLabel[kLabelCount] = {};
  const char* const kGone[] = {
      " GENRE", " LENGTH", "TARGET", "OUT", "USB", "LIVE MIDI", "CHANNEL", "ROUTE", "ROLE",
      "LEAD MODE", "PARTNER", " BARS", " HOLD", " EVOLVE", " REPLACE", " LP BARS",
      " LP CHANNEL", " LP LENGTH", " LP HOLD", " LP LAYER", " LP MUTE", " LP > MELODY",
      "LOCK LAYER",
  };
  partner.setMode(Listener::Mode::Loop);  // RIFF values visible
  for (uint8_t r = 0; r <= kLast; ++r) {
    goInput(base, r);
    gfx.texts.clear();
    page.drawContent(gfx);  // the tools layer; the header may name the project genre
    for (const char* gone : kGone) assert(!gfx.sawExact(gone));
    assert(!gfx.saw("AUTO=PROJECT") && !gfx.saw("ANSWER LENGTH: 100"));
    for (int i = 0; i < kLabelCount; ++i) sawLabel[i] |= gfx.sawExact(kLabels[i]);
    // Every label fits left of the value column (COL_2 = 120, label at x 10).
    for (const auto& t : gfx.texts) assert(t.size() <= 38);
  }
  for (int i = 0; i < kLabelCount; ++i) {
    if (!sawLabel[i]) std::printf("missing label %s\n", kLabels[i]);
    assert(sawLabel[i]);
  }
  // The [LOOP] tab: nine rows, six visible, a scroll bar; whichever row is
  // selected its label is on screen, and TO MELODY (the last row) is reachable.
  {
    const char* const kLoopLabels[] = {"LOOPER", "REC", "LOOP BARS", "LOOP CH", "NOTE LENGTH",
                                       "HOLD LAYER", "LAYER", "MUTE LAYER", "TO MELODY"};
    static_assert(sizeof(kLoopLabels) / sizeof(kLoopLabels[0]) == PerformPage::kLoopRowCount, "labels");
    bool sawLoopLabel[PerformPage::kLoopRowCount] = {};
    for (uint8_t r = 0; r < PerformPage::kLoopRowCount; ++r) {
      goLoop(base, static_cast<PerformLoopRow>(r));
      gfx.texts.clear();
      gfx.rects.clear();
      page.drawContent(gfx);
      assert(gfx.sawExact(kLoopLabels[r]));  // the selected row is on screen
      for (uint8_t i = 0; i < PerformPage::kLoopRowCount; ++i) sawLoopLabel[i] |= gfx.sawExact(kLoopLabels[i]);
      assert(gfx.sawExact("[LOOP]"));
      bool bar = false;
      for (const auto& rc : gfx.rects) bar |= (rc.w == 2 && rc.x >= 236 && rc.x + rc.w <= 240);
      assert(bar);  // scroll bar inside the 240 px screen
      // The window never shows more than six rows.
      int rowsShown = 0;
      for (const char* label : kLoopLabels) rowsShown += gfx.sawExact(label) ? 1 : 0;
      assert(rowsShown <= 6);
    }
    for (bool seen : sawLoopLabel) assert(seen);
    goLoop(base, L::Looper);
    send(base, nav(GROOVEPUTER_UP));  // wraps within the nine rows
    assert(loopRow(base) == performLoopRow(L::ToMelody));
    gfx.texts.clear();
    page.drawContent(gfx);
    assert(gfx.sawExact("TO MELODY") && gfx.sawExact("MUTE LAYER") && !gfx.sawExact("LOOPER"));
    send(base, nav(GROOVEPUTER_DOWN));
    assert(loopRow(base) == 0);
  }
  // HELPER shows RIFF for the repeating mode; its values are words.
  goInput(base, row(R::Helper));
  gfx.texts.clear();
  page.drawContent(gfx);
  assert(gfx.sawExact("RIFF"));
  {
    // The word LOOP is only the inactive [LOOP] tab label, never a HELPER value.
    int loopWords = 0;
    for (const auto& t : gfx.texts) loopWords += t == "LOOP" ? 1 : 0;
    assert(loopWords == 1);
  }
  goInput(base, row(R::NewRiffIf));  // window: HELPER .. NEW RIFF IF
  gfx.texts.clear();
  page.drawContent(gfx);
  assert(gfx.sawExact("RIFF"));
  assert(gfx.sawExact("DX ROUTE"));  // REPLY CH 0 (same word as LOOP CH 0)
  assert(!gfx.sawExact("AS KEYS"));
  assert(gfx.sawExact("ENDING"));   // VARIATION default 1
  assert(gfx.sawExact("3 NOTES"));  // NEW RIFF IF default
  // Up/Down never land on a heading: from LEAD STYLE one DOWN is HELPER, and
  // back with UP.
  goInput(base, row(R::LeadStyle));
  send(base, nav(GROOVEPUTER_DOWN));
  assert(inputRow(base) == row(R::Helper));
  send(base, nav(GROOVEPUTER_UP));
  assert(inputRow(base) == row(R::LeadStyle));
  send(base, nav(GROOVEPUTER_DOWN));
  assert(inputRow(base) == row(R::Helper));
  // The selected row and its heading are on screen at the top of a group.
  gfx.texts.clear();
  page.drawContent(gfx);
  assert(gfx.sawExact("--- HELPER ---") && gfx.sawExact("HELPER"));
  // The list is longer than the window: a 2 px scroll bar on the right edge
  // (track + thumb), inside the 240 px screen.
  {
    bool bar = false;
    for (const auto& r : gfx.rects) {
      if (r.w == 2 && r.x + r.w <= 240 && r.x >= 236) bar = true;
    }
    assert(bar);
  }
  // MIDI IN hint points to PROJECT > MIDI for the USB role.
  goInput(base, row(R::MidiIn));
  gfx.texts.clear();
  page.drawContent(gfx);
  assert(gfx.saw("USB: PROJECT>MIDI"));
  assert(gfx.sawExact("--- MIDI IN ---"));
  // MIDI IN rows still act: LEFT/RIGHT on PLAY AS cycles the role, LEAD STYLE
  // follows LEAD only.
  goInput(base, row(R::PlayAs));
  const auto roleBefore = keyboard.performanceRole();
  send(base, nav(GROOVEPUTER_RIGHT));
  assert(keyboard.performanceRole() != roleBefore);
  send(base, nav(GROOVEPUTER_LEFT));
  assert(keyboard.performanceRole() == roleBefore);

  // The renumbered rows act on what they name; </> and ENTER on any PARTNER
  // row never touch GENRE or LENGTH.
  partner.setMode(Listener::Mode::Loop);
  goInput(base, row(R::ReplyBars));
  const uint8_t bars = partner.answerBars();
  send(base, nav(GROOVEPUTER_RIGHT));
  assert(partner.answerBars() != bars);
  send(base, nav(GROOVEPUTER_LEFT));
  assert(partner.answerBars() == bars);
  assertDefaults(partner);

  goInput(base, row(R::ReplyChannel));
  const uint8_t channel = partner.answerChannel();
  send(base, nav(GROOVEPUTER_RIGHT));
  assert(partner.answerChannel() != channel);
  send(base, nav(GROOVEPUTER_LEFT));
  assert(partner.answerChannel() == channel);
  assertDefaults(partner);

  goInput(base, row(R::Variation));
  const uint8_t evolve = partner.loopEvolve();
  send(base, nav(GROOVEPUTER_RIGHT));
  assert(partner.loopEvolve() != evolve);
  send(base, nav(GROOVEPUTER_LEFT));
  assert(partner.loopEvolve() == evolve);
  assertDefaults(partner);

  goInput(base, row(R::NewRiffIf));
  const auto replace = partner.loopReplace();
  send(base, nav(GROOVEPUTER_RIGHT));
  assert(partner.loopReplace() != replace);
  send(base, nav(GROOVEPUTER_LEFT));
  assert(partner.loopReplace() == replace);
  assertDefaults(partner);

  // FREEZE RIFF: ENTER toggles the listener hold (no phrase yet: stays off).
  goInput(base, row(R::FreezeRiff));
  send(base, key('\r'));
  assert(!partner.loopHold());
  for (uint8_t r = row(R::ReplyBars); r <= row(R::NewRiffIf); ++r) {
    goInput(base, r);
    send(base, nav(GROOVEPUTER_RIGHT));
    send(base, nav(GROOVEPUTER_LEFT));
    send(base, key('\r'));
    assertDefaults(partner);
  }
  // HELPER still cycles the PARTNER mode (OFF -> ANSWER -> ...).
  partner.setMode(Listener::Mode::Off);
  goInput(base, row(R::Helper));
  send(base, nav(GROOVEPUTER_RIGHT));
  assert(partner.mode() == Listener::Mode::Answer);
  // While the PARTNER is on, the LOOPER refuses Z (they share one player) and
  // says how to leave: HELPER OFF FIRST; the LOOPER row reads HELPER ON.
  auto& loop = GroovePuterLooper::looper();
  send(base, key('z'));
  assert(loop.state() == Looper::State::Idle);
  gfx.texts.clear();
  UI::drawToast(gfx);
  assert(gfx.saw("LOOP: HELPER OFF FIRST"));
  goLoop(base, L::Looper);
  gfx.texts.clear();
  page.drawContent(gfx);
  assert(gfx.sawExact("HELPER ON"));
  goInput(base, row(R::Helper));
  send(base, nav(GROOVEPUTER_LEFT));
  assert(partner.mode() == Listener::Mode::Off);

  // LOOPER rows act on the looper, never on the PARTNER settings.
  goLoop(base, L::RecordSource);
  assert(!loop.recordSound());
  send(base, nav(GROOVEPUTER_RIGHT));
  assert(loop.recordSound());
  gfx.texts.clear();
  page.drawContent(gfx);
  assert(gfx.sawExact("SOUND"));
  assert(loop.recDub() == Looper::Result::Done);
  send(base, nav(GROOVEPUTER_RIGHT));
  assert(loop.recordSound());
  gfx.texts.clear();
  UI::drawToast(gfx);
  assert(gfx.saw("LOOP: STOP REC TO CHANGE"));
  assert(loop.recDub() == Looper::Result::Done);
  send(base, nav(GROOVEPUTER_LEFT));
  assert(!loop.recordSound());
  const uint8_t partnerBars = partner.answerBars();
  const uint8_t partnerChannel = partner.answerChannel();
  goLoop(base, L::LoopBars);
  const uint8_t lpBars = loop.bars();
  assert(lpBars == 2);
  send(base, nav(GROOVEPUTER_RIGHT));
  send(base, nav(GROOVEPUTER_RIGHT));
  assert(loop.bars() == 8 && loop.passSteps() == 128);
  send(base, nav(GROOVEPUTER_LEFT));
  send(base, nav(GROOVEPUTER_LEFT));
  assert(loop.bars() == lpBars);
  goLoop(base, L::LoopChannel);
  const uint8_t lpChannel = loop.channel();
  send(base, nav(GROOVEPUTER_RIGHT));
  assert(loop.channel() != lpChannel);
  send(base, nav(GROOVEPUTER_LEFT));
  assert(loop.channel() == lpChannel);
  goLoop(base, L::NoteLength);
  const uint8_t lpLength = loop.lengthPercent();
  send(base, nav(GROOVEPUTER_RIGHT));
  assert(loop.lengthPercent() != lpLength);
  send(base, nav(GROOVEPUTER_LEFT));
  assert(loop.lengthPercent() == lpLength);
  assert(partner.answerBars() == partnerBars && partner.answerChannel() == partnerChannel);
  assert(partner.mode() == Listener::Mode::Off);
  assertDefaults(partner);

  // LOOP CH 0 reads DX ROUTE; HOLD LAYER on an empty loop: nothing to hold.
  goLoop(base, L::LoopChannel);
  gfx.texts.clear();
  page.drawContent(gfx);
  assert(gfx.sawExact("DX ROUTE") && !gfx.sawExact("AS KEYS"));
  goLoop(base, L::HoldLayer);
  send(base, key('\r'));
  assert(!loop.hold());

  // Ctrl+Z with nothing to undo in the LOOPER is passed on (global toast).
  {
    UIEvent undo{};
    undo.event_type = GROOVEPUTER_APPLICATION_EVENT;
    undo.app_event_type = GROOVEPUTER_APP_EVENT_UNDO;
    assert(!loop.canUndo());
    assert(!base.handleEvent(undo));
  }

  // LOOPER row: ENTER = Z (IDLE -> ARMED -> IDLE).
  goLoop(base, L::Looper);
  assert(loop.state() == Looper::State::Idle);
  send(base, key('\r'));
  assert(loop.state() == Looper::State::Armed);
  gfx.texts.clear();
  page.drawContent(gfx);
  assert(gfx.sawExact("ARM"));
  send(base, key('\r'));
  assert(loop.state() == Looper::State::Idle);

  // LAYER: ENTER = M; M also works with the tools layer open. MUTE LAYER.
  goLoop(base, L::Layer);
  assert(loop.activeLayer() == 0);
  send(base, key('\r'));
  assert(loop.activeLayer() == 1);
  send(base, nav(GROOVEPUTER_RIGHT));
  assert(loop.activeLayer() == 0);
  send(base, key('m'));
  assert(loop.activeLayer() == 1);
  send(base, key('M'));
  assert(loop.activeLayer() == 0);
  goLoop(base, L::MuteLayer);
  send(base, key('\r'));
  assert(loop.layerMuted(0) && !loop.layerMuted(1));
  send(base, nav(GROOVEPUTER_LEFT));
  assert(loop.muteMask() == 0);
  assertDefaults(partner);
  assert(partner.mode() == Listener::Mode::Off);

  // Micro-timing mark on the step grids: none at TIMING 0, a 2x2 in the left top
  // corner when early, in the right one when late.
  {
    gfx.rects.clear();
    StepTimingMark::draw(gfx, 10, 20, 12, 2, 0, IGfxColor(0xFFFFFF));
    assert(gfx.rects.empty());
    StepTimingMark::draw(gfx, 10, 20, 12, 2, -3, IGfxColor(0xFFFFFF));
    assert(gfx.rects.size() == 1);
    assert(gfx.rects[0].x == 12 && gfx.rects[0].y == 22 && gfx.rects[0].w == 2 && gfx.rects[0].h == 2);
    StepTimingMark::draw(gfx, 10, 20, 12, 2, 5, IGfxColor(0xFFFFFF));
    assert(gfx.rects.size() == 2);
    assert(gfx.rects[1].x == 18 && gfx.rects[1].y == 22);  // 10 + 12 - 2 - 2
    gfx.rects.clear();
  }

  // TO MELODY target: LEFT/RIGHT on the TO MELODY row picks synth A / B (default: the
  // GEN TARGET voice); the pick never touches GEN TARGET; the export toast names
  // the picked synth and a pick after the first ENTER disarms the confirmation.
  {
    GroovePuterState::cycleGenerationTarget(1);  // MELODY A
    const auto genTarget = GroovePuterState::generationTarget();
    const int genVoice = GroovePuterState::melodyTargetVoice();
    assert(genVoice == 0);
    send(base, key('z'));  // ARM, then the first note starts the loop
    loop.onPerformanceInputNote(true, 1, 60, 100);
    loop.onPerformanceInputNote(false, 1, 0, 0);
    assert(!loop.empty());
    goLoop(base, L::ToMelody);
    auto drawn = [&](const char* text) {
      gfx.texts.clear();
      page.drawContent(gfx);
      return gfx.sawExact(text);
    };
    assert(drawn("SYN A"));              // follows GEN TARGET until picked
    send(base, nav(GROOVEPUTER_RIGHT));
    assert(drawn("SYN B"));
    send(base, nav(GROOVEPUTER_LEFT));
    assert(drawn("SYN A"));
    send(base, nav(GROOVEPUTER_LEFT));
    assert(drawn("SYN B"));
    assert(GroovePuterState::melodyTargetVoice() == genVoice);
    assert(GroovePuterState::generationTarget() == genTarget);
    send(base, key('\r'));
    gfx.texts.clear();
    UI::drawToast(gfx);
    assert(gfx.saw("REPLACE MELODY B WITH LOOP? ENTER"));
    send(base, nav(GROOVEPUTER_RIGHT));  // pick A: the armed export is for B, so ENTER re-arms
    send(base, key('\r'));
    gfx.texts.clear();
    UI::drawToast(gfx);
    assert(gfx.saw("REPLACE MELODY A WITH LOOP? ENTER"));
    assert(GroovePuterState::melodyTargetVoice() == genVoice);

    // Behaviour of the confirmation (armed for A now): a mute change drops it,
    // the second ENTER (same state) takes the loop, and an unsaved Working
    // Melody in the target is then refused without touching it.
    auto toastHas = [&](const char* text) {
      gfx.texts.clear();
      UI::drawToast(gfx);
      return gfx.saw(text);
    };
    engine.init();  // the export commits into a real voice (the page holds a reference)
    engine.setSongMode(false);
    GroovePuterUndo::undoOwner().clear();
    assert(!engine.hasUnsavedWorkingMelody(0));
    goLoop(base, L::MuteLayer);
    send(base, key('\r'));  // the only layer with notes is muted
    goLoop(base, L::ToMelody);
    send(base, key('\r'));
    assert(toastHas("TO MELODY: ALL LAYERS MUTED"));  // refused, confirmation dropped
    goLoop(base, L::MuteLayer);
    send(base, key('\r'));  // unmuted again: same mask as when armed, still not armed
    goLoop(base, L::ToMelody);
    send(base, key('\r'));
    assert(toastHas("REPLACE MELODY A WITH LOOP? ENTER"));  // a question again, nothing taken
    assert(!engine.hasUnsavedWorkingMelody(0));
    send(base, key('\r'));  // the second ENTER takes it
    assert(toastHas("MELODY A <- LOOP  CTRL+Z ON SYNTH A"));
    assert(engine.hasUnsavedWorkingMelody(0));
    send(base, key('\r'));
    assert(toastHas("SAVE OR DISCARD A FIRST"));  // never overwrites an unsaved melody
    send(base, key('\r'));
    assert(toastHas("SAVE OR DISCARD A FIRST"));  // and no armed confirmation was left behind
  }

  // View continuity: the last row (19) survives a page round trip; a stale
  // row past the list (20..22) falls back to the top.
  goInput(base, kLast);
  UI::UiViewContinuityState saved = capture(base);
  PerformPage other(gfx, engine, keyboard);
  IPage& otherBase = other;
  otherBase.restoreViewContinuity(saved);
  assert(inputRow(otherBase) == kLast);
  for (uint8_t stale = kLast + 1; stale <= kLast + 3; ++stale) {
    saved.performRows[static_cast<int>(PerformanceToolContext::Input)] = stale;
    otherBase.restoreViewContinuity(saved);
    assert(inputRow(otherBase) == 0);
  }
  assertDefaults(partner);

  // View continuity of the [LOOP] tab (0.9.23 T14): the context and the row of
  // the LOOP tab and of the INPUT tab are kept separately; a stale LOOP row
  // (6..8) falls back to the top; a stale context falls back to KEY.
  goLoop(base, L::MuteLayer);
  saved = capture(base);
  assert(saved.performContext == static_cast<uint8_t>(PerformanceToolContext::Loop));
  assert(saved.performToolsVisible == 1);
  assert(saved.performRows[static_cast<int>(PerformanceToolContext::Loop)] == performLoopRow(L::MuteLayer));
  PerformPage third(gfx, engine, keyboard);
  IPage& thirdBase = third;
  thirdBase.restoreViewContinuity(saved);
  assert(loopRow(thirdBase) == performLoopRow(L::MuteLayer));
  for (uint8_t stale = PerformPage::kLoopRowCount; stale <= PerformPage::kLoopRowCount + 2; ++stale) {
    saved.performRows[static_cast<int>(PerformanceToolContext::Loop)] = stale;
    thirdBase.restoreViewContinuity(saved);
    assert(loopRow(thirdBase) == 0);
  }
  saved.performContext = 7;  // beyond the tabs
  thirdBase.restoreViewContinuity(saved);
  assert(contextOf(thirdBase) == static_cast<uint8_t>(PerformanceToolContext::Key));

  std::puts("cr phase10 menu tests passed");
  return 0;
}
