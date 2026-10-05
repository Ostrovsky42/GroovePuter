// PROJECT -> MIDI: the USB role row. L/R previews, ENTER saves (the running role does not change),
// a second ENTER restarts; a project with unsaved changes is never restarted from here.
#include <cassert>
#include <cstdio>
#include <cstring>

#include "src/platform/cardputer_usb_role_runtime.h"
#include "src/state/scene_revision.h"
#include "src/ui/pages/project_page.h"
#include "src/ui/ui_common.h"

SerialMock Serial;
SDMock SD;

namespace {
class NullGfx : public IGfx {
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
  int textWidth(const char* t) const override { return t ? static_cast<int>(std::strlen(t)) * 6 : 0; }
  int fontHeight() const override { return 8; }
  int width() const override { return 240; }
  int height() const override { return 135; }
};

UIEvent key(char k) {
  UIEvent e{};
  e.event_type = GROOVEPUTER_KEY_DOWN;
  e.key = k;
  return e;
}
UIEvent scan(KeyScanCode c) {
  UIEvent e{};
  e.event_type = GROOVEPUTER_KEY_DOWN;
  e.scancode = c;
  return e;
}
}  // namespace

int main() {
  MiniAcid engine(44100.0f, nullptr);
  NullGfx gfx;
  ProjectPage page(gfx, engine, AudioGuard{});
  page.onEnter(0);
  auto send = [&](UIEvent e) { (void)page.handleEvent(e); };

  assert(CardputerUsbRoleRuntime::activeRole() == UsbBootRole::Device);  // compatible default
  assert(CardputerUsbRoleRuntime::pendingRole() == UsbBootRole::Device);
  assert(!CardputerUsbRoleRuntime::restartPending());

  for (int i = 0; i < 3; ++i) send(key('\t'));  // Scenes -> Groove -> Led -> Midi: first row is USB

  // L/R only previews: nothing is saved yet.
  send(scan(GROOVEPUTER_RIGHT));
  assert(CardputerUsbRoleRuntime::pendingRole() == UsbBootRole::Device);
  // ENTER saves; the running role is untouched, a restart is pending.
  send(key('\n'));
  assert(CardputerUsbRoleRuntime::pendingRole() == UsbBootRole::Host);
  assert(CardputerUsbRoleRuntime::activeRole() == UsbBootRole::Device);
  assert(CardputerUsbRoleRuntime::restartPending());
  assert(CardputerUsbRoleRuntime::restartRequests() == 0);

  // A second ENTER would restart, but unsaved changes block it (no silent data loss).
  GroovePuterState::markSceneMutated();
  assert(GroovePuterState::sceneDirty());
  send(key('\n'));
  assert(CardputerUsbRoleRuntime::restartRequests() == 0);

  // Once the project is saved, the second ENTER requests the restart.
  GroovePuterState::markSceneSaveSucceeded();
  send(key('\n'));
  assert(CardputerUsbRoleRuntime::restartRequests() == 1);
  assert(CardputerUsbRoleRuntime::pendingRole() == UsbBootRole::Host);

  // Choosing the running role again cancels the pending change (no restart needed).
  send(scan(GROOVEPUTER_RIGHT));  // KEYBOARD -> OFF
  send(scan(GROOVEPUTER_RIGHT));  // OFF -> COMPUTER
  send(key('\n'));
  assert(CardputerUsbRoleRuntime::pendingRole() == UsbBootRole::Device);
  assert(!CardputerUsbRoleRuntime::restartPending());

  // OFF is selectable and saved like the others.
  send(scan(GROOVEPUTER_LEFT));  // COMPUTER -> OFF
  send(key('\n'));
  assert(CardputerUsbRoleRuntime::pendingRole() == UsbBootRole::Off);
  assert(CardputerUsbRoleRuntime::restartPending());

  std::puts("PROJECT USB role row: PASS");
  return 0;
}
