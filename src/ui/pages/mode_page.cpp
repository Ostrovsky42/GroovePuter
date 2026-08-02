#include "mode_page.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "../layout_manager.h"
#include "../ui_common.h"
#include "../ui_input.h"
#include "../../dsp/atlas_runtime.h"
#include "../../dsp/groove_profile.h"
#include "../../dsp/phrase_generator.h"

namespace {
uint8_t atlasVariationForRole(PhraseGenerator::PhraseBarRole role) {
  switch (role) {
    case PhraseGenerator::PhraseBarRole::Base:
    case PhraseGenerator::PhraseBarRole::Return:
      return 0;
    case PhraseGenerator::PhraseBarRole::MicroVariation:
    case PhraseGenerator::PhraseBarRole::Development:
    case PhraseGenerator::PhraseBarRole::Build:
      return 1;
    case PhraseGenerator::PhraseBarRole::Breakdown:
    case PhraseGenerator::PhraseBarRole::Fill:
    case PhraseGenerator::PhraseBarRole::EndingFill:
      return 2;
  }
  return 0;
}
}  // namespace

ModePage::ModePage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard)
    : mini_acid_(mini_acid), audio_guard_(audio_guard) {
  (void)gfx;
}

const char* ModePage::modeName(GrooveboxMode mode) const {
  switch (mode) {
    case GrooveboxMode::Acid: return "ACID";
    case GrooveboxMode::Minimal: return "MINIMAL";
    case GrooveboxMode::Breaks: return "BREAKS";
    case GrooveboxMode::Dub: return "DUB";
    case GrooveboxMode::Electro: return "ELECTRO";
    default: return "MINIMAL";
  }
}

const char* ModePage::flavorName(GrooveboxMode mode, int flavor) const {
  if (flavor < 0) flavor = 0;
  if (flavor > 4) flavor = 4;
  static const char* acid[5] = {"CLASSIC", "SHARP", "DEEP", "RUBBER", "RAVE"};
  static const char* minimal[5] = {"TIGHT", "WARM", "AIRY", "DRY", "HYPNO"};
  static const char* breaks[5] = {"NUSKOOL", "SKITTER", "ROLLER", "CRUNCH", "LIQUID"};
  static const char* dub[5] = {"HEAVY", "SPACE", "STEPPERS", "TAPE", "FOG"};
  static const char* electro[5] = {"ROBOT", "ZAP", "BOING", "MIAMI", "INDUS"};
  switch (mode) {
    case GrooveboxMode::Acid: return acid[flavor];
    case GrooveboxMode::Minimal: return minimal[flavor];
    case GrooveboxMode::Breaks: return breaks[flavor];
    case GrooveboxMode::Dub: return dub[flavor];
    case GrooveboxMode::Electro: return electro[flavor];
    default: return minimal[flavor];
  }
}

void ModePage::drawRow(IGfx& gfx, int y, const char* label, const char* value, bool focused, IGfxColor accent) {
  const int x = Layout::CONTENT.x;
  const int w = Layout::CONTENT.w;
  const int h = 10;
  if (focused) {
    gfx.drawRect(x, y - 1, w, h, accent);
  }
  gfx.setTextColor(IGfxColor(0x8AA4BA));
  gfx.drawText(x + 2, y + 1, label);
  gfx.setTextColor(focused ? accent : COLOR_WHITE);
  gfx.drawText(x + 56, y + 1, value);
}

void ModePage::draw(IGfx& gfx) {
  const ModeConfig& cfg = mini_acid_.modeManager().config();
  const GrooveboxMode mode = mini_acid_.grooveboxMode();
  const int flavor = mini_acid_.grooveFlavor();
  const bool macros = mini_acid_.sceneManager().currentScene().genre.applySoundMacros;
  const float delayMix = std::max(mini_acid_.tempoDelay(0).mixValue(), mini_acid_.tempoDelay(1).mixValue());
  const float tapeSpace = mini_acid_.sceneManager().currentScene().tape.space / 100.0f;
  PatternCorridors c = GrooveProfile::getCorridors(mode, flavor);
  const PatternCorridors before = c;
  GrooveProfile::applyBudgetRules(mode, delayMix, tapeSpace, c);
  const bool ducked = (c.notesMin != before.notesMin) || (c.notesMax != before.notesMax) ||
                      (c.accentProbability != before.accentProbability);

  char title[48];
  std::snprintf(title, sizeof(title), "%s / %s", modeName(mode), flavorName(mode, flavor));
  UI::drawStandardHeader(gfx, mini_acid_, title);
  LayoutManager::clearContent(gfx);

  const int y0 = LayoutManager::lineY(0);
  drawRow(gfx, y0, "MODE", modeName(mode), focus_ == FocusRow::Mode, cfg.accentColor);

  char flv[24];
  std::snprintf(flv, sizeof(flv), "%s [%d/5]", flavorName(mode, flavor), flavor + 1);
  drawRow(gfx, y0 + Layout::LINE_HEIGHT, "FLAVOR", flv, focus_ == FocusRow::Flavor, cfg.accentColor);

  char phraseLength[24];
  std::snprintf(phraseLength, sizeof(phraseLength), "%u BARS -> SONG", static_cast<unsigned>(phrase_bars_));
  drawRow(gfx, y0 + Layout::LINE_HEIGHT * 2, "PHRASE", phraseLength,
          focus_ == FocusRow::PhraseLength, cfg.accentColor);

  char generateLabel[40];
  std::snprintf(generateLabel, sizeof(generateLabel), "ENTER @ ROW %d",
                std::max(0, mini_acid_.currentSongPosition()) + 1);
  drawRow(gfx, y0 + Layout::LINE_HEIGHT * 3, "GENERATE", generateLabel,
          focus_ == FocusRow::GeneratePhrase, cfg.accentColor);

  drawRow(gfx, y0 + Layout::LINE_HEIGHT * 4, "MACROS",
          macros ? "ON (Flavor -> Sound)" : "OFF (Safe)",
          focus_ == FocusRow::Macros, cfg.accentColor);

  char corridorLine[96];
  std::snprintf(corridorLine, sizeof(corridorLine), "N %d..%d A %.0f%% S %.0f%% SW %.0f%%",
                c.notesMin, c.notesMax, c.accentProbability * 100.0f,
                c.slideProbability * 100.0f, c.swingAmount * 100.0f);
  gfx.setTextColor(IGfxColor(0x8AA4BA));
  gfx.drawText(Layout::CONTENT.x + 2, y0 + Layout::LINE_HEIGHT * 5 + 1, corridorLine);

  char budgetLine[64];
  std::snprintf(budgetLine, sizeof(budgetLine), "BUDGET %s dly %.2f spc %.2f",
                ducked ? "DUCK" : "FREE", delayMix, tapeSpace);
  gfx.setTextColor(ducked ? cfg.accentColor : IGfxColor(0x5C7183));
  gfx.drawText(Layout::CONTENT.x + 2, y0 + Layout::LINE_HEIGHT * 6 + 1, budgetLine);

  UI::drawStandardFooter(gfx, "TAB/ARW:Edit  ENT:Action", "SPACE:Preview  A/B/D:Apply");
}

void ModePage::moveFocus(int delta) {
  constexpr int kFocusCount = 5;
  int f = static_cast<int>(focus_) + delta;
  while (f < 0) f += kFocusCount;
  while (f >= kFocusCount) f -= kFocusCount;
  focus_ = static_cast<FocusRow>(f);
}

void ModePage::toggleMode() {
  withAudioGuard([&]() { mini_acid_.toggleGrooveboxMode(); });
  char toast[64];
  std::snprintf(toast, sizeof(toast), "Groove: %s (override)",
                modeName(mini_acid_.grooveboxMode()));
  UI::showToast(toast, 1100);
}

void ModePage::shiftMode(int delta) {
  int idx = static_cast<int>(mini_acid_.grooveboxMode());
  idx += delta;
  while (idx < 0) idx += 5;
  while (idx >= 5) idx -= 5;
  withAudioGuard([&]() { mini_acid_.setGrooveboxMode(static_cast<GrooveboxMode>(idx)); });
  char toast[64];
  std::snprintf(toast, sizeof(toast), "Groove: %s (override)",
                modeName(mini_acid_.grooveboxMode()));
  UI::showToast(toast, 900);
}

void ModePage::shiftFlavor(int delta) {
  withAudioGuard([&]() { mini_acid_.shiftGrooveFlavor(delta); });
}

void ModePage::shiftPhraseLength(int delta) {
  static constexpr uint8_t kLengths[4] = {1, 2, 4, 8};
  int index = 0;
  for (int i = 0; i < 4; ++i) {
    if (kLengths[i] == phrase_bars_) {
      index = i;
      break;
    }
  }
  index = (index + delta + 4) % 4;
  phrase_bars_ = kLengths[index];
}

void ModePage::generatePhrase() {
  const bool wasPlaying = mini_acid_.isPlaying();
  if (wasPlaying) mini_acid_.stop();

  PhraseGenerator::PhraseResult result{};
  withAudioGuard([&]() {
    PhraseGenerator::PhraseRequest request{};
    request.bars = phrase_bars_;
    request.songStart = std::max(0, mini_acid_.currentSongPosition());
    request.pageIndex = mini_acid_.currentPageIndex();
    request.seed = static_cast<uint32_t>(rand());

    Scene& scene = mini_acid_.sceneManager().currentScene();
    const GenreRecipeId recipe = mini_acid_.genreManager().recipe();
    const bool atlasPhrase = AtlasRuntime::hasRecipe(recipe) &&
                             AtlasRuntime::variationCount(recipe) >= 3;

    if (atlasPhrase) {
      result = PhraseGenerator::generateBarsToSong(
          scene, request,
          [&](PhraseGenerator::PhraseBar& bar,
              PhraseGenerator::PhraseBarRole role,
              int barIndex) {
            (void)barIndex;
            return AtlasRuntime::applyRecipe(
                recipe, atlasVariationForRole(role),
                bar.synthA, bar.synthB, bar.drums, nullptr);
          });
    } else {
      result = PhraseGenerator::generateToSong(
          scene, request, [&](PhraseGenerator::PhraseBar& base) {
            const GenerativeParams& params =
                mini_acid_.genreManager().getCompiledGenerativeParams();
            const GenreBehavior behavior = mini_acid_.genreManager().getBehavior();
            mini_acid_.modeManager().generatePattern(
                base.synthA, mini_acid_.bpm(), params, behavior, 0);
            mini_acid_.modeManager().generatePattern(
                base.synthB, mini_acid_.bpm(), params, behavior, 1);
            mini_acid_.modeManager().generateDrumPattern(
                base.drums, params, behavior);
          });
    }

    if (result) {
      mini_acid_.setSongMode(true);
      mini_acid_.setSongPlaybackSlot(scene.activeSongSlot);
      mini_acid_.setSongPosition(result.songStart);
    }
  });

  if (result) {
    char toast[80];
    std::snprintf(toast, sizeof(toast), "%dB phrase -> Song %d..%d (rows 1B)",
                  result.bars, result.songStart + 1,
                  result.songStart + result.bars);
    UI::showToast(toast, 1800);
  } else {
    UI::showToast(PhraseGenerator::errorText(result.error), 1600);
  }

  if (wasPlaying) mini_acid_.start();
}

void ModePage::applyTo303(int voiceIdx) {
  withAudioGuard([&]() { mini_acid_.modeManager().apply303Preset(voiceIdx, mini_acid_.grooveFlavor()); });
}

void ModePage::applyToDrums() {
  withAudioGuard([&]() {
    mini_acid_.randomizeDrumPattern();
  });
}

void ModePage::previewMode() {
  bool wasPlaying = mini_acid_.isPlaying();
  if (wasPlaying) {
      mini_acid_.stop();
  }

  withAudioGuard([&]() {
    mini_acid_.randomize303Pattern(0);
    mini_acid_.randomize303Pattern(1);
    mini_acid_.randomizeDrumPattern();
  });

  if (wasPlaying) {
      mini_acid_.start();
  } else {
      mini_acid_.start();
  }
}

void ModePage::toggleMacros() {
  withAudioGuard([&]() {
    auto& genre = mini_acid_.sceneManager().currentScene().genre;
    genre.applySoundMacros = !genre.applySoundMacros;
  });
}

bool ModePage::handleEvent(UIEvent& ui_event) {
  if (ui_event.event_type != GROOVEPUTER_KEY_DOWN) return false;

  if (UIInput::isTab(ui_event)) {
    moveFocus(1);
    return true;
  }

  const int nav = UIInput::navCode(ui_event);
  if (nav == GROOVEPUTER_UP) {
    moveFocus(-1);
    return true;
  }
  if (nav == GROOVEPUTER_DOWN) {
    moveFocus(1);
    return true;
  }
  if (nav == GROOVEPUTER_LEFT || nav == GROOVEPUTER_RIGHT) {
    const int delta = (nav == GROOVEPUTER_RIGHT) ? 1 : -1;
    switch (focus_) {
      case FocusRow::Mode: shiftMode(delta); return true;
      case FocusRow::Flavor: shiftFlavor(delta); return true;
      case FocusRow::PhraseLength: shiftPhraseLength(delta); return true;
      case FocusRow::GeneratePhrase: return true;
      case FocusRow::Macros: toggleMacros(); return true;
    }
  }

  char key = ui_event.key;
  if (!key) return false;

  switch (key) {
    case '\n':
    case '\r':
      switch (focus_) {
        case FocusRow::Mode: toggleMode(); return true;
        case FocusRow::Flavor: shiftFlavor(1); return true;
        case FocusRow::PhraseLength: shiftPhraseLength(1); return true;
        case FocusRow::GeneratePhrase: generatePhrase(); return true;
        case FocusRow::Macros: toggleMacros(); return true;
      }
      break;
    case 'a':
    case 'A':
      applyTo303(0);
      return true;
    case 'b':
    case 'B':
      applyTo303(1);
      return true;
    case 'd':
    case 'D':
      applyToDrums();
      return true;
    case 'g':
    case 'G':
      generatePhrase();
      return true;
    case 'm':
    case 'M':
      toggleMacros();
      return true;
    case ' ':
      previewMode();
      return true;
    default:
      break;
  }

  return false;
}

const std::string& ModePage::getTitle() const {
  return title_;
}
