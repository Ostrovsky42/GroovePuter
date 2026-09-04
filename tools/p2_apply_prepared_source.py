#!/usr/bin/env python3
from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise SystemExit(
            f"{path}: expected one replacement, found {count}: {old[:100]!r}"
        )
    p.write_text(text.replace(old, new, 1), encoding="utf-8")


replace_once(
    "src/dsp/miniacid_engine.h",
    '#include "pattern_drum_event_tap.h"\n#include "tube_distortion.h"',
    '#include "pattern_drum_event_tap.h"\n'
    '#include "../phrase/runtime_pattern_event_bank.h"\n'
    '#include "tube_distortion.h"',
)
replace_once(
    "src/dsp/miniacid_engine.h",
    "  void setCurrentPage(int8_t page) { currentPage_.store(page, std::memory_order_release); }\n"
    "  void requestPageSwitch(int pageIndex);",
    "  void setCurrentPage(int8_t page) { currentPage_.store(page, std::memory_order_release); }\n"
    "  bool rebuildPatternRuntimeEventBank();\n"
    "  bool refreshPatternRuntimeEvents(int synthIndex, int bankIndex, int patternIndex);\n"
    "  const PhraseRuntime::RuntimePatternEventBuffer& activePatternRuntimeEvents(int synthIndex) const;\n"
    "  void requestPageSwitch(int pageIndex);",
)
replace_once(
    "src/dsp/miniacid_engine.h",
    "  ClampedLiveNoteIdentity liveNotes_[NUM_303_VOICES] = {-1, -1};\n"
    "  PatternEventQueueHandle patternEventQueue_;",
    "  ClampedLiveNoteIdentity liveNotes_[NUM_303_VOICES] = {-1, -1};\n"
    "  PhraseRuntime::RuntimePatternEventBank patternRuntimeBank_;\n"
    "  PatternEventQueueHandle patternEventQueue_;",
)

replace_once(
    "src/dsp/miniacid_engine.cpp",
    '#include "../audio/audio_diagnostics.h"\n#include "../input/musical_event_queue.h"',
    '#include "../audio/audio_diagnostics.h"\n'
    '#include "../audio/pattern_paging.h"\n'
    '#include "../input/musical_event_queue.h"',
)

engine_runtime = r'''bool MiniAcid::rebuildPatternRuntimeEventBank() {
  const int residentPage = PatternPagingService::activePageIndex();
  if (residentPage < 0 || residentPage >= kMaxPages) return false;

  const Scene& scene = sceneManager_.currentScene();
  const auto recipe = genreManager_.getGrooveRecipe();
  const int swingPct = std::clamp(
      static_cast<int>(scene.feel.swingPct), 50, 75);

  PhraseRuntime::RuntimePatternEventBank candidate{};
  for (uint8_t synth = 0; synth < NUM_303_VOICES; ++synth) {
    PhraseRuntime::PatternProjectionSettings settings{};
    settings.synthIndex = synth;
    settings.gateLengthRatio = recipe.gateLengthRatio;
    settings.swingPercent = static_cast<uint8_t>(swingPct);
    const VoiceId voice = synth == 0 ? VoiceId::SynthA : VoiceId::SynthB;
    settings.swingEnabled =
        (scene.feel.swingMask & (1u << static_cast<int>(voice))) != 0;

    for (uint8_t bank = 0; bank < kBankCount; ++bank) {
      for (uint8_t pattern = 0;
           pattern < Bank<SynthPattern>::kPatterns;
           ++pattern) {
        const SynthPattern& source = synth == 0
            ? scene.synthABanks[bank].patterns[pattern]
            : scene.synthBBanks[bank].patterns[pattern];
        if (candidate.refresh(synth, bank, pattern, source, settings) !=
            PhraseRuntime::PatternBankRefreshStatus::Ready) {
          return false;
        }
      }
    }
  }

  if (!candidate.publishPageIdentity(residentPage)) return false;
  patternRuntimeBank_ = candidate;
  return true;
}

bool MiniAcid::refreshPatternRuntimeEvents(int synthIndex,
                                           int bankIndex,
                                           int patternIndex) {
  if (synthIndex < 0 || synthIndex >= NUM_303_VOICES ||
      bankIndex < 0 || bankIndex >= kBankCount ||
      patternIndex < 0 || patternIndex >= Bank<SynthPattern>::kPatterns) {
    return false;
  }

  const int residentPage = PatternPagingService::activePageIndex();
  if (residentPage != currentPageIndex() ||
      patternRuntimeBank_.pageIdentity() != residentPage) {
    return false;
  }

  const Scene& scene = sceneManager_.currentScene();
  const auto recipe = genreManager_.getGrooveRecipe();
  PhraseRuntime::PatternProjectionSettings settings{};
  settings.synthIndex = static_cast<uint8_t>(synthIndex);
  settings.gateLengthRatio = recipe.gateLengthRatio;
  settings.swingPercent = static_cast<uint8_t>(std::clamp(
      static_cast<int>(scene.feel.swingPct), 50, 75));
  const VoiceId voice = synthIndex == 0 ? VoiceId::SynthA : VoiceId::SynthB;
  settings.swingEnabled =
      (scene.feel.swingMask & (1u << static_cast<int>(voice))) != 0;

  const SynthPattern& source = synthIndex == 0
      ? scene.synthABanks[bankIndex].patterns[patternIndex]
      : scene.synthBBanks[bankIndex].patterns[patternIndex];
  return patternRuntimeBank_.refresh(
             static_cast<uint8_t>(synthIndex),
             static_cast<uint8_t>(bankIndex),
             static_cast<uint8_t>(patternIndex),
             source,
             settings) == PhraseRuntime::PatternBankRefreshStatus::Ready;
}

const PhraseRuntime::RuntimePatternEventBuffer&
MiniAcid::activePatternRuntimeEvents(int synthIndex) const {
  if (synthIndex < 0 || synthIndex >= NUM_303_VOICES) {
    return patternRuntimeBank_.empty();
  }
  const int bankIndex = current303BankIndex(synthIndex);
  const int patternIndex = current303PatternIndex(synthIndex);
  if (bankIndex < 0 || bankIndex >= kBankCount ||
      patternIndex < 0 || patternIndex >= Bank<SynthPattern>::kPatterns) {
    return patternRuntimeBank_.empty();
  }
  return patternRuntimeBank_.selectForPage(
      currentPageIndex(),
      static_cast<uint8_t>(synthIndex),
      static_cast<uint8_t>(bankIndex),
      static_cast<uint8_t>(patternIndex));
}

'''
replace_once(
    "src/dsp/miniacid_engine.cpp",
    "void MiniAcid::updateDrumReverbDecay(float value) {",
    engine_runtime + "void MiniAcid::updateDrumReverbDecay(float value) {",
)

replace_once(
    "src/ui/pages/pattern_edit_page.h",
    "          const auto apply = [&]() {\n"
    "            GroovePuterUndo::restoreSynthPatternUndo(manager, prepared);\n"
    "          };",
    "          const auto apply = [&]() {\n"
    "            GroovePuterUndo::restoreSynthPatternUndo(manager, prepared);\n"
    "            (void)mini_acid_.refreshPatternRuntimeEvents(\n"
    "                prepared.synthIndex, prepared.bankIndex, prepared.patternIndex);\n"
    "          };",
)

replace_once(
    "src/ui/pages/pattern_edit_page_legacy.h",
    "              const auto restore = [&]() {\n"
    "                GroovePuterUndo::exchangeSynthPatternUndo(\n"
    "                    mini_acid_.sceneManager(), receipt);\n"
    "              };",
    "              const auto restore = [&]() {\n"
    "                GroovePuterUndo::exchangeSynthPatternUndo(\n"
    "                    mini_acid_.sceneManager(), receipt);\n"
    "                (void)mini_acid_.refreshPatternRuntimeEvents(\n"
    "                    receipt.synthIndex, receipt.bankIndex, receipt.patternIndex);\n"
    "              };",
)

replace_once(
    "src/ui/pages/pattern_edit_page.cpp",
    "                const auto exchange = [&]() {\n"
    "                  GroovePuterUndo::exchangeSynthPatternUndo(\n"
    "                      mini_acid_.sceneManager(), receipt);\n"
    "                };",
    "                const auto exchange = [&]() {\n"
    "                  GroovePuterUndo::exchangeSynthPatternUndo(\n"
    "                      mini_acid_.sceneManager(), receipt);\n"
    "                  (void)mini_acid_.refreshPatternRuntimeEvents(\n"
    "                      receipt.synthIndex, receipt.bankIndex, receipt.patternIndex);\n"
    "                };",
)

replace_once(
    "src/ui/pages/synth_sequencer_page.cpp",
    "          const auto apply = [&]() {\n"
    "            GroovePuterUndo::restoreSynthPatternUndo(manager, prepared);\n"
    "          };",
    "          const auto apply = [&]() {\n"
    "            GroovePuterUndo::restoreSynthPatternUndo(manager, prepared);\n"
    "            (void)mini_acid_.refreshPatternRuntimeEvents(\n"
    "                prepared.synthIndex, prepared.bankIndex, prepared.patternIndex);\n"
    "          };",
)

replace_once(
    "src/ui/pages/feel_page.cpp",
    "          scene.feel.swingPct = next;\n"
    "          mini_acid_.applyFeelTimingFromScene_();\n"
    "          changed = true;",
    "          scene.feel.swingPct = next;\n"
    "          mini_acid_.applyFeelTimingFromScene_();\n"
    "          (void)mini_acid_.rebuildPatternRuntimeEventBank();\n"
    "          changed = true;",
)
replace_once(
    "src/ui/pages/feel_page.cpp",
    "      mini_acid_.applyFeelTimingFromScene_();\n"
    "    });\n"
    "    GroovePuterState::markSceneMutated();",
    "      mini_acid_.applyFeelTimingFromScene_();\n"
    "      (void)mini_acid_.rebuildPatternRuntimeEventBank();\n"
    "    });\n"
    "    GroovePuterState::markSceneMutated();",
)

old_paging = '''        } else if (PatternPagingService::pageExists(target)) {
            if (PatternPagingService::loadPage(target, scene)) {
                mini_acid_.setCurrentPage(target);
                result = PageSwitchResult::Switched;
            } else {
                result = PageSwitchResult::LoadTargetFailed;
            }
        } else {
            PatternPagingService::initializeEmptyPage(scene);
            if (PatternPagingService::savePage(target, scene)) {
                mini_acid_.setCurrentPage(target);
                result = PageSwitchResult::Created;
            } else if (PatternPagingService::loadPage(current, scene)) {
                result = PageSwitchResult::CreateTargetFailed;
            } else {
                result = PageSwitchResult::RollbackFailed;
            }
        }'''
new_paging = '''        } else if (PatternPagingService::pageExists(target)) {
            if (PatternPagingService::loadPage(target, scene)) {
                if (mini_acid_.rebuildPatternRuntimeEventBank()) {
                    mini_acid_.setCurrentPage(target);
                    result = PageSwitchResult::Switched;
                } else if (PatternPagingService::loadPage(current, scene)) {
                    result = PageSwitchResult::LoadTargetFailed;
                } else {
                    result = PageSwitchResult::RollbackFailed;
                }
            } else {
                result = PageSwitchResult::LoadTargetFailed;
            }
        } else {
            PatternPagingService::initializeEmptyPage(scene);
            if (PatternPagingService::savePage(target, scene)) {
                if (mini_acid_.rebuildPatternRuntimeEventBank()) {
                    mini_acid_.setCurrentPage(target);
                    result = PageSwitchResult::Created;
                } else if (PatternPagingService::loadPage(current, scene)) {
                    result = PageSwitchResult::CreateTargetFailed;
                } else {
                    result = PageSwitchResult::RollbackFailed;
                }
            } else if (PatternPagingService::loadPage(current, scene)) {
                result = PageSwitchResult::CreateTargetFailed;
            } else {
                result = PageSwitchResult::RollbackFailed;
            }
        }'''
replace_once("src/ui/miniacid_display.cpp", old_paging, new_paging)
