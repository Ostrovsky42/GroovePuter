#!/usr/bin/env python3
from pathlib import Path

path = Path("src/dsp/miniacid_engine.cpp")
text = path.read_text()

old = '''  if (PhraseRuntime::projectPatternToRuntimeEvents(
          activeSynthPattern(voiceIndex), settings, candidate) !=
      PhraseRuntime::PatternProjectionStatus::Ready) {
    return false;
  }

  workingMaterial_[voiceIndex].storeMelody(candidate);
'''

new = '''  if (PhraseRuntime::projectPatternToRuntimeEvents(
          activeSynthPattern(voiceIndex), settings, candidate) !=
      PhraseRuntime::PatternProjectionStatus::Ready) {
    return false;
  }

  // Pattern lifetime is cyclic: a late onset may legitimately sustain through
  // the bar boundary into step 0. A Phrase/Melody is linear, so MAKE PHRASE is
  // the ownership boundary where that cyclic tail must be bounded to the new
  // object's terminal extent. Do not change Pattern projection semantics.
  const uint32_t phraseEndSubtick =
      static_cast<uint32_t>(candidate.lengthTicks) *
      PhraseRuntime::kSubticksPerTick;
  for (uint16_t i = 0; i < candidate.count; ++i) {
    auto& event = candidate.events[i];
    const uint32_t startSubtick =
        static_cast<uint32_t>(event.startTick) *
        PhraseRuntime::kSubticksPerTick;
    if (startSubtick >= phraseEndSubtick) return false;
    const uint32_t maxDuration = phraseEndSubtick - startSubtick;
    if (event.durationSubticks > maxDuration) {
      event.durationSubticks = static_cast<uint16_t>(maxDuration);
    }
  }
  if (!RuntimePhraseEdit::validate(candidate)) return false;

  workingMaterial_[voiceIndex].storeMelody(candidate);
'''

if new in text:
    print("C9 MAKE PHRASE boundary fix already present")
    raise SystemExit(0)

count = text.count(old)
if count != 1:
    raise SystemExit(f"expected exactly one MAKE PHRASE patch site, found {count}")

path.write_text(text.replace(old, new, 1))
print("C9 MAKE PHRASE boundary fix applied")
