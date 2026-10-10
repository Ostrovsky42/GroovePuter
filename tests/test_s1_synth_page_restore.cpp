// 0.9.19 S1 (coredump 2026-10-10): a recreated Synth page restored its
// remembered MORE tab before its components existed and wrote through a null
// component pointer (StoreProhibited at 0x18) -> panic on returning to SYNTH B.
#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

#include "platform_sdl/scene_storage_sdl.h"
#include "src/audio/audio_config.h"
#include "src/audio/pattern_paging.h"
#include "src/dsp/miniacid_engine.h"
#include "src/platform/cardputer_material_publication_session.h"
#include "src/ui/pages/tb303_params_page.h"

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
  int textWidth(const char* s) const override { return s ? std::strlen(s) * 6 : 0; }
  int fontHeight() const override { return 8; }
  int width() const override { return 240; }
  int height() const override { return 135; }
};
}  // namespace

int main() {
  const auto root = std::filesystem::temp_directory_path() / "gp_s1_synth_page_restore";
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root);
  std::filesystem::current_path(root);
  SD.setRoot(root);
  GroovePuterPlatform::clearMaterialPublication("s1page", 0);
  PatternPagingService::setProjectName("s1page");
  SceneStorageSdl storage;
  storage.setCurrentSceneName("default");
  MiniAcid engine(static_cast<float>(kSampleRate), &storage);
  engine.init();
  NullGfx gfx;

  for (int voice = 0; voice < 2; ++voice) {
    TB303ParamsPage page(gfx, engine, AudioGuard{}, voice);
    page.showMoreTab(true);  // restoreViewContinuity() order: before bounds
    page.setBoundaries(Rect{0, 0, gfx.width(), gfx.height()});
    page.draw(gfx);
    page.showMoreTab(false);
    page.draw(gfx);
    page.showMoreTab(true);
    page.draw(gfx);
  }
  std::puts("S1: a Synth page restores the MORE tab before its components exist: PASS");
  return 0;
}
