#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, value: str) -> None:
    (ROOT / path).write_text(value, encoding="utf-8")


def replace_once(path: str, old: str, new: str) -> None:
    value = read(path)
    count = value.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one exact match, got {count}: {old[:100]!r}")
    write(path, value.replace(old, new, 1))


BANK = "src/phrase/runtime_pattern_event_bank.h"
replace_once(
    BANK,
    "struct RuntimePatternEventBuffer {\n"
    "  RuntimeSynthEvent events[kPatternRuntimeMaxEvents]{};\n"
    "  uint8_t count = 0;\n\n"
    "  constexpr uint16_t lengthTicks() const { return kTicksPerBar; }\n"
    "};\n",
    "struct RuntimePatternEventBuffer {\n"
    "  RuntimeSynthEvent events[kPatternRuntimeMaxEvents]{};\n"
    "  uint8_t count = 0;\n"
    "  uint16_t onsetMask = 0;\n\n"
    "  constexpr uint16_t lengthTicks() const { return kTicksPerBar; }\n\n"
    "  const RuntimeSynthEvent* eventForSourceStep(uint8_t sourceStep) const {\n"
    "    if (sourceStep >= SynthPattern::kSteps ||\n"
    "        (onsetMask & static_cast<uint16_t>(1u << sourceStep)) == 0) {\n"
    "      return nullptr;\n"
    "    }\n"
    "    uint16_t preceding = sourceStep == 0\n"
    "        ? 0\n"
    "        : static_cast<uint16_t>(onsetMask & ((1u << sourceStep) - 1u));\n"
    "    uint8_t eventIndex = 0;\n"
    "    while (preceding != 0) {\n"
    "      eventIndex = static_cast<uint8_t>(eventIndex + (preceding & 1u));\n"
    "      preceding = static_cast<uint16_t>(preceding >> 1u);\n"
    "    }\n"
    "    return eventIndex < count ? &events[eventIndex] : nullptr;\n"
    "  }\n"
    "};\n",
)
replace_once(
    BANK,
    "    RuntimePatternEventBuffer candidate{};\n"
    "    candidate.count = static_cast<uint8_t>(projected.count);\n"
    "    for (uint8_t i = 0; i < candidate.count; ++i) {\n"
    "      candidate.events[i] = projected.events[i];\n"
    "    }\n",
    "    RuntimePatternEventBuffer candidate{};\n"
    "    candidate.count = static_cast<uint8_t>(projected.count);\n"
    "    uint8_t projectedOrdinal = 0;\n"
    "    for (uint8_t step = 0; step < SynthPattern::kSteps; ++step) {\n"
    "      if (pattern.steps[step].note < 0) continue;\n"
    "      candidate.onsetMask = static_cast<uint16_t>(\n"
    "          candidate.onsetMask | static_cast<uint16_t>(1u << step));\n"
    "      if (projectedOrdinal >= candidate.count) {\n"
    "        return PatternBankRefreshStatus::ProjectionFailed;\n"
    "      }\n"
    "      candidate.events[projectedOrdinal] = projected.events[projectedOrdinal];\n"
    "      ++projectedOrdinal;\n"
    "    }\n"
    "    if (projectedOrdinal != candidate.count) {\n"
    "      return PatternBankRefreshStatus::ProjectionFailed;\n"
    "    }\n",
)
replace_once(
    BANK,
    "static_assert(sizeof(RuntimePatternEventBuffer) <= 162,\n"
    "              \"retained Pattern event buffer exceeded its fixed budget\");\n",
    "static_assert(sizeof(RuntimePatternEventBuffer) <= 164,\n"
    "              \"retained Pattern event buffer exceeded its fixed budget\");\n",
)

CPP = "src/dsp/miniacid_engine.cpp"
old = '''  // P2 Synth A/B: prepared immutable events are authoritative for onset
  // placement. Invalid/wrong-page bank selection is canonical silence.
  const uint32_t absoluteStartSubtick =
      absoluteTick * static_cast<uint32_t>(PhraseRuntime::kSubticksPerTick);
  for (int synth = 0; synth < NUM_303_VOICES; ++synth) {
    const PhraseRuntime::RuntimePatternEventBuffer& events =
        activePatternRuntimeEvents(synth);
    for (uint8_t i = 0; i < events.count; ++i) {
      const PhraseRuntime::RuntimeSynthEvent& event = events.events[i];
      if (event.startTick != barTick) continue;
      triggerSynthStep_(synth, event, absoluteStartSubtick);
    }
  }

  // Drums retain the accepted legacy timing path in P2 executor cutover.
  int swingPct = GroovePuterRhythm::QuantizedGenerationDetail::audibleGenerationSwingPct(
      *this, sceneManager_.currentScene().feel.swingPct);
  if (swingPct < 50) swingPct = 50;
  if (swingPct > 75) swingPct = 75;
  int swingDelay = (int)std::round((swingPct - 50.0f) * 24.0f / 50.0f);
  uint16_t swingMask = sceneManager_.currentScene().feel.swingMask;

  int nominalStep = barTick / 24;
  for (int sIdx = nominalStep - 1; sIdx <= nominalStep + 1; ++sIdx) {
    int s = (sIdx + 16) % 16;
    uint32_t nominalT = s * 24;
    const DrumPatternSet* pendingDrums =
        GroovePuterRhythm::QuantizedGenerationDetail::pendingAudibleDrumPatternSet(*this);
    const DrumPatternSet& dSet = pendingDrums
        ? *pendingDrums
        : sceneManager_.getCurrentDrumPattern();
    for (int v = 0; v < 8; ++v) {
      VoiceId vId = (VoiceId)((int)VoiceId::DrumKick + v);
      int swingD = (s % 2 != 0 && (swingMask & (1 << (int)vId))) ? swingDelay : 0;
      int microD = dSet.voices[v].steps[s].timing;
      if ((nominalT + swingD + microD + 384) % 384 == barTick) {
        triggerDrumVoice_(v, s);
      }
    }
  }
'''
new = '''  // P2 changes the Synth material/lifetime source, not legacy trigger/RNG
  // ordering. Keep the physical source-step scan and A -> B -> drums order.
  const uint32_t absoluteStartSubtick =
      absoluteTick * static_cast<uint32_t>(PhraseRuntime::kSubticksPerTick);
  const PhraseRuntime::RuntimePatternEventBuffer& synthAEvents =
      activePatternRuntimeEvents(0);
  const PhraseRuntime::RuntimePatternEventBuffer& synthBEvents =
      activePatternRuntimeEvents(1);

  int swingPct = GroovePuterRhythm::QuantizedGenerationDetail::audibleGenerationSwingPct(
      *this, sceneManager_.currentScene().feel.swingPct);
  if (swingPct < 50) swingPct = 50;
  if (swingPct > 75) swingPct = 75;
  int swingDelay = (int)std::round((swingPct - 50.0f) * 24.0f / 50.0f);
  uint16_t swingMask = sceneManager_.currentScene().feel.swingMask;

  int nominalStep = barTick / 24;
  for (int sIdx = nominalStep - 1; sIdx <= nominalStep + 1; ++sIdx) {
    int s = (sIdx + 16) % 16;
    uint32_t nominalT = s * 24;

    if (const PhraseRuntime::RuntimeSynthEvent* eventA =
            synthAEvents.eventForSourceStep(static_cast<uint8_t>(s));
        eventA != nullptr && eventA->startTick == barTick) {
      triggerSynthStep_(0, *eventA, absoluteStartSubtick);
    }
    if (const PhraseRuntime::RuntimeSynthEvent* eventB =
            synthBEvents.eventForSourceStep(static_cast<uint8_t>(s));
        eventB != nullptr && eventB->startTick == barTick) {
      triggerSynthStep_(1, *eventB, absoluteStartSubtick);
    }

    const DrumPatternSet* pendingDrums =
        GroovePuterRhythm::QuantizedGenerationDetail::pendingAudibleDrumPatternSet(*this);
    const DrumPatternSet& dSet = pendingDrums
        ? *pendingDrums
        : sceneManager_.getCurrentDrumPattern();
    for (int v = 0; v < 8; ++v) {
      VoiceId vId = (VoiceId)((int)VoiceId::DrumKick + v);
      int swingD = (s % 2 != 0 && (swingMask & (1 << (int)vId))) ? swingDelay : 0;
      int microD = dSet.voices[v].steps[s].timing;
      if ((nominalT + swingD + microD + 384) % 384 == barTick) {
        triggerDrumVoice_(v, s);
      }
    }
  }
'''
replace_once(CPP, old, new)

P0 = "tests/test_pattern_phrase_p0_source_contract.py"
replace_once(
    P0,
    '    "activePatternRuntimeEvents" in sequencer\n    and "event.startTick" in sequencer,\n'
    '    "P2 Synth scheduling is not driven by prepared runtime event startTick",\n',
    '    "activePatternRuntimeEvents" in sequencer\n    and "eventForSourceStep" in sequencer\n    and "->startTick" in sequencer,\n'
    '    "P2 Synth scheduling is not driven by prepared runtime source-step events",\n',
)

print("P2 source-step ordering correction applied")
