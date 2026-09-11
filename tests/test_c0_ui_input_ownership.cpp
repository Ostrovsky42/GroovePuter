#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "arduino_compat.h"
#include "../src/audio/audio_config.h"
#include "../src/dsp/miniacid_engine.h"
#include "../src/ui/pages/song_page.h"
#include "../src/ui/workflow_mode.h"

SerialMock Serial;
SDMock SD;

namespace {

class RecordingGfx : public IGfx {
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

int failures = 0;

void expect(bool condition, const char* message) {
  if (condition) return;
  std::fprintf(stderr, "C0 UI input ownership FAIL: %s\n", message);
  ++failures;
}

UIEvent altLetter(char key, KeyScanCode scancode) {
  UIEvent event{};
  event.event_type = GROOVEPUTER_KEY_DOWN;
  event.key = key;
  event.scancode = scancode;
  event.alt = true;
  return event;
}

void expectSongPhraseHandoff(UIEvent event, const char* source) {
  MiniAcid engine(kSampleRate, nullptr);
  RecordingGfx gfx;
  SongPage page(gfx, engine, AudioGuard{});

  const bool handled = page.handleEvent(event);
  char message[160];

  std::snprintf(message, sizeof(message), "%s Alt+J was not consumed by SongPage", source);
  expect(handled, message);
  std::snprintf(message, sizeof(message), "%s Alt+J did not request a page transition", source);
  expect(page.hasPageRequest(), message);
  std::snprintf(message, sizeof(message), "%s Alt+J did not target PHRASE", source);
  expect(page.getRequestedPage() == WorkflowPages::kPhrase, message);
  std::snprintf(message, sizeof(message), "%s Alt+J did not hand off cursor row as one-shot context", source);
  expect(page.getRequestedContext() == 1, message);
}

void testSongPhraseHandoffAcceptsPrintableKey() {
  expectSongPhraseHandoff(altLetter('j', GROOVEPUTER_NO_SCANCODE), "printable-key");
}

void testSongPhraseHandoffAcceptsPhysicalScancode() {
  expectSongPhraseHandoff(altLetter(0, GROOVEPUTER_J), "scancode-only");
}

}  // namespace

int main() {
  testSongPhraseHandoffAcceptsPrintableKey();
  testSongPhraseHandoffAcceptsPhysicalScancode();

  if (failures == 0) {
    std::puts("C0 UI input ownership: PASS");
    return 0;
  }
  std::fprintf(stderr, "C0 UI input ownership: %d failure(s)\n", failures);
  return 1;
}
