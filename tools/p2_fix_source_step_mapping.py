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
        raise RuntimeError(
            f"{path}: expected one exact match, got {count}: {old[:120]!r}")
    write(path, value.replace(old, new, 1))


HEADER = "src/phrase/runtime_synth_events.h"
replace_once(
    HEADER,
    "PatternProjectionStatus projectPatternToRuntimeEvents(\n"
    "    const SynthPattern& pattern,\n"
    "    const PatternProjectionSettings& settings,\n"
    "    RuntimeSynthEventBuffer& destination);\n",
    "PatternProjectionStatus projectPatternToRuntimeEvents(\n"
    "    const SynthPattern& pattern,\n"
    "    const PatternProjectionSettings& settings,\n"
    "    RuntimeSynthEventBuffer& destination);\n\n"
    "// P2 companion projection metadata. The existing RuntimeSynthEvent ABI and\n"
    "// chronological RuntimeSynthEventBuffer order remain unchanged; this helper\n"
    "// only exposes which physical Pattern step produced each projected onset.\n"
    "PatternProjectionStatus projectPatternToRuntimeEventsWithSourceSteps(\n"
    "    const SynthPattern& pattern,\n"
    "    const PatternProjectionSettings& settings,\n"
    "    RuntimeSynthEventBuffer& destination,\n"
    "    uint8_t (&sourceSteps)[SynthPattern::kSteps]);\n",
)

CPP = "src/phrase/runtime_synth_events.cpp"
old_function = '''PatternProjectionStatus projectPatternToRuntimeEvents(
    const SynthPattern& pattern,
    const PatternProjectionSettings& settings,
    RuntimeSynthEventBuffer& destination) {
  if (settings.synthIndex >= 2) {
    return PatternProjectionStatus::InvalidSynthIndex;
  }

  RuntimeSynthEventBuffer candidate{};
  candidate.lengthTicks = kTicksPerBar;

  const TriggerTokenBuffer tokens = collectTriggerTokens(pattern, settings);
  const uint16_t baseDuration = baseGateDurationSubticks(settings);

  uint8_t eventTokenIndices[SynthPattern::kSteps]{};
  for (uint8_t tokenIndex = 0; tokenIndex < tokens.count; ++tokenIndex) {
    const TriggerToken& token = tokens.values[tokenIndex];
    if (token.note < 0) continue;
    if (candidate.count >= kMaxSynthEvents) break;

    const SynthStep& step = pattern.steps[token.stepIndex];
    RuntimeSynthEvent& event = candidate.events[candidate.count];
    event.startTick = token.tick;
    event.durationSubticks = baseDuration;
    event.note = static_cast<uint8_t>(token.note);
    event.velocity = step.velocity;
    event.probability = step.probability;
    event.flags = eventFlags(step);
    event.fx = step.fx;
    event.fxParam = step.fxParam;
    eventTokenIndices[candidate.count] = tokenIndex;
    ++candidate.count;
  }

  for (uint16_t eventIndex = 0; eventIndex < candidate.count; ++eventIndex) {
    foldLegacyLifetime(
        pattern,
        tokens,
        eventTokenIndices[eventIndex],
        baseDuration,
        candidate.events[eventIndex]);
  }

  destination = candidate;
  return PatternProjectionStatus::Ready;
}
'''
new_function = '''namespace {

PatternProjectionStatus projectPatternToRuntimeEventsImpl(
    const SynthPattern& pattern,
    const PatternProjectionSettings& settings,
    RuntimeSynthEventBuffer& destination,
    uint8_t* sourceSteps) {
  if (settings.synthIndex >= 2) {
    return PatternProjectionStatus::InvalidSynthIndex;
  }

  RuntimeSynthEventBuffer candidate{};
  candidate.lengthTicks = kTicksPerBar;

  const TriggerTokenBuffer tokens = collectTriggerTokens(pattern, settings);
  const uint16_t baseDuration = baseGateDurationSubticks(settings);

  uint8_t eventTokenIndices[SynthPattern::kSteps]{};
  for (uint8_t tokenIndex = 0; tokenIndex < tokens.count; ++tokenIndex) {
    const TriggerToken& token = tokens.values[tokenIndex];
    if (token.note < 0) continue;
    if (candidate.count >= kMaxSynthEvents) break;

    const SynthStep& step = pattern.steps[token.stepIndex];
    RuntimeSynthEvent& event = candidate.events[candidate.count];
    event.startTick = token.tick;
    event.durationSubticks = baseDuration;
    event.note = static_cast<uint8_t>(token.note);
    event.velocity = step.velocity;
    event.probability = step.probability;
    event.flags = eventFlags(step);
    event.fx = step.fx;
    event.fxParam = step.fxParam;
    eventTokenIndices[candidate.count] = tokenIndex;
    if (sourceSteps != nullptr && candidate.count < SynthPattern::kSteps) {
      sourceSteps[candidate.count] = token.stepIndex;
    }
    ++candidate.count;
  }

  for (uint16_t eventIndex = 0; eventIndex < candidate.count; ++eventIndex) {
    foldLegacyLifetime(
        pattern,
        tokens,
        eventTokenIndices[eventIndex],
        baseDuration,
        candidate.events[eventIndex]);
  }

  destination = candidate;
  return PatternProjectionStatus::Ready;
}

}  // namespace

PatternProjectionStatus projectPatternToRuntimeEvents(
    const SynthPattern& pattern,
    const PatternProjectionSettings& settings,
    RuntimeSynthEventBuffer& destination) {
  return projectPatternToRuntimeEventsImpl(
      pattern, settings, destination, nullptr);
}

PatternProjectionStatus projectPatternToRuntimeEventsWithSourceSteps(
    const SynthPattern& pattern,
    const PatternProjectionSettings& settings,
    RuntimeSynthEventBuffer& destination,
    uint8_t (&sourceSteps)[SynthPattern::kSteps]) {
  for (uint8_t& sourceStep : sourceSteps) {
    sourceStep = 0xFFu;
  }
  return projectPatternToRuntimeEventsImpl(
      pattern, settings, destination, sourceSteps);
}
'''
replace_once(CPP, old_function, new_function)

BANK = "src/phrase/runtime_pattern_event_bank.h"
old_bank = '''    RuntimeSynthEventBuffer projected{};
    if (projectPatternToRuntimeEvents(pattern, settings, projected) !=
        PatternProjectionStatus::Ready) {
      return PatternBankRefreshStatus::ProjectionFailed;
    }
    if (projected.count > kPatternRuntimeMaxEvents) {
      return PatternBankRefreshStatus::ProjectionFailed;
    }

    RuntimePatternEventBuffer candidate{};
    candidate.count = static_cast<uint8_t>(projected.count);
    uint8_t projectedOrdinal = 0;
    for (uint8_t step = 0; step < SynthPattern::kSteps; ++step) {
      if (pattern.steps[step].note < 0) continue;
      candidate.onsetMask = static_cast<uint16_t>(
          candidate.onsetMask | static_cast<uint16_t>(1u << step));
      if (projectedOrdinal >= candidate.count) {
        return PatternBankRefreshStatus::ProjectionFailed;
      }
      candidate.events[projectedOrdinal] = projected.events[projectedOrdinal];
      ++projectedOrdinal;
    }
    if (projectedOrdinal != candidate.count) {
      return PatternBankRefreshStatus::ProjectionFailed;
    }
'''
new_bank = '''    RuntimeSynthEventBuffer projected{};
    uint8_t projectedSourceSteps[SynthPattern::kSteps]{};
    if (projectPatternToRuntimeEventsWithSourceSteps(
            pattern, settings, projected, projectedSourceSteps) !=
        PatternProjectionStatus::Ready) {
      return PatternBankRefreshStatus::ProjectionFailed;
    }
    if (projected.count > kPatternRuntimeMaxEvents) {
      return PatternBankRefreshStatus::ProjectionFailed;
    }

    // P1C keeps chronological projected order. P2 compact retention needs
    // physical source-step order so the executor can preserve legacy
    // step -> Synth A -> Synth B -> drums RNG ordering without reading
    // mutable SynthPattern material in AudioTask.
    RuntimePatternEventBuffer candidate{};
    for (uint8_t step = 0; step < SynthPattern::kSteps; ++step) {
      for (uint8_t projectedIndex = 0;
           projectedIndex < projected.count;
           ++projectedIndex) {
        if (projectedSourceSteps[projectedIndex] != step) continue;
        if (candidate.count >= kPatternRuntimeMaxEvents) {
          return PatternBankRefreshStatus::ProjectionFailed;
        }
        candidate.onsetMask = static_cast<uint16_t>(
            candidate.onsetMask | static_cast<uint16_t>(1u << step));
        candidate.events[candidate.count++] = projected.events[projectedIndex];
        break;
      }
    }
    if (candidate.count != projected.count) {
      return PatternBankRefreshStatus::ProjectionFailed;
    }
'''
replace_once(BANK, old_bank, new_bank)

TEST = "tests/test_pattern_phrase_p2_pattern_bank.cpp"
old_test = '''void assertMatchesP1CProjection(const SynthPattern& pattern,
                                const PatternProjectionSettings& settings,
                                const RuntimePatternEventBuffer& compact) {
  RuntimeSynthEventBuffer reference{};
  assert(projectPatternToRuntimeEvents(pattern, settings, reference) ==
         PatternProjectionStatus::Ready);
  assert(reference.count <= SynthPattern::kSteps);
  assert(compact.count == reference.count);
  assert(compact.lengthTicks() == kTicksPerBar);
  for (uint8_t i = 0; i < compact.count; ++i) {
    assert(std::memcmp(&compact.events[i], &reference.events[i],
                       sizeof(RuntimeSynthEvent)) == 0);
  }
}
'''
new_test = '''void assertMatchesP1CProjection(const SynthPattern& pattern,
                                const PatternProjectionSettings& settings,
                                const RuntimePatternEventBuffer& compact) {
  RuntimeSynthEventBuffer reference{};
  uint8_t sourceSteps[SynthPattern::kSteps]{};
  assert(projectPatternToRuntimeEventsWithSourceSteps(
             pattern, settings, reference, sourceSteps) ==
         PatternProjectionStatus::Ready);
  assert(reference.count <= SynthPattern::kSteps);
  assert(compact.count == reference.count);
  assert(compact.lengthTicks() == kTicksPerBar);
  for (uint8_t i = 0; i < reference.count; ++i) {
    const RuntimeSynthEvent* retained = compact.eventForSourceStep(sourceSteps[i]);
    assert(retained != nullptr);
    assert(std::memcmp(retained, &reference.events[i],
                       sizeof(RuntimeSynthEvent)) == 0);
  }
}
'''
replace_once(TEST, old_test, new_test)

print("P2 source-step mapping companion projection applied")
