#!/usr/bin/env python3
"""One-time guarded migration for the streaming scene parser."""

from pathlib import Path


SOURCE = Path("scenes.cpp")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected 1 occurrence, found {count}")
    return text.replace(old, new, 1)


def main() -> None:
    text = SOURCE.read_text(encoding="utf-8")

    text = replace_once(
        text,
        """    pattern.steps[i].accent = false;
    pattern.steps[i].probability = 100;
""",
        """    pattern.steps[i].accent = false;
    pattern.steps[i].ghost = false;
    pattern.steps[i].velocity = 100;
    pattern.steps[i].timing = 0;
    pattern.steps[i].probability = 100;
""",
        "clear synth metadata",
    )

    text = replace_once(
        text,
        """  for (int i = 0; i < Song::kMaxPositions; ++i) {
    for (int t = 0; t < SongPosition::kTrackCount; ++t) {
      song.positions[i].patterns[t] = -1;
    }
  }
""",
        """  for (int i = 0; i < Song::kMaxPositions; ++i) {
    for (int t = 0; t < SongPosition::kTrackCount; ++t) {
      song.positions[i].patterns[t] = -1;
    }
  }
  song.length = 1;
  song.reverse = false;
""",
        "clear song metadata",
    )

    text = replace_once(
        text,
        """  clearCustomPhrases(scene);
  scene.masterVolume = 0.6f;
  scene.generatorParams = GeneratorParams();
  scene.led = LedSettings();
  scene.tape = TapeState();
  scene.feel = FeelSettings();
  scene.drumFX = DrumFX();
""",
        """  clearCustomPhrases(scene);
  for (auto& pad : scene.samplerPads) pad = SamplerPadState();
  for (float& volume : scene.trackVolumes) volume = 1.0f;
  scene.masterVolume = 0.6f;
  scene.generatorParams = GeneratorParams();
  scene.led = LedSettings();
  scene.tape = TapeState();
  scene.feel = FeelSettings();
  scene.genre = GenreSettings();
  scene.vocal = VocalSettings();
  scene.drumFX = DrumFX();
  scene.activeSongSlot = 0;
  scene.mode = GrooveboxMode::Minimal;
  scene.grooveFlavor = 0;
""",
        "clear complete scene defaults",
    )

    text = replace_once(
        text,
        """        else if (lastKey_ == "prb") path = Path::DrumProbabilityArray;
        else if (lastKey_ == "fx") path = Path::DrumFxArray;
""",
        """        else if (lastKey_ == "prb") path = Path::DrumProbabilityArray;
        else if (lastKey_ == "vel") path = Path::DrumVelocityArray;
        else if (lastKey_ == "tim") path = Path::DrumTimingArray;
        else if (lastKey_ == "fx") path = Path::DrumFxArray;
""",
        "drum array paths",
    )

    text = replace_once(
        text,
        """    } else if (lastKey_ == "lofiAmt") {
      if (v < 0) v = 0;
""",
        """    } else if (lastKey_ == "swing") {
      if (v < 50) v = 50;
      if (v > 75) v = 75;
      target_.feel.swingPct = static_cast<uint8_t>(v);
    } else if (lastKey_ == "mask") {
      if (v < 0) v = 0;
      if (v > 0xFFFF) v = 0xFFFF;
      target_.feel.swingMask = static_cast<uint16_t>(v);
    } else if (lastKey_ == "lofiAmt") {
      if (v < 0) v = 0;
""",
        "feel swing parser",
    )

    old_drum = """  if (path == Path::DrumFxArray || path == Path::DrumFxParamArray || path == Path::DrumProbabilityArray) {
    int bankIdx = currentIndexFor(Path::DrumBanks);
    if (bankIdx < 0) bankIdx = 0;
    int patternIdx = currentIndexFor(Path::DrumBank);
    int voiceIdx = currentIndexFor(Path::DrumPatternSet);
    int stepIdx = stack_[stackSize_ - 1].index;
    if (patternIdx >= 0 && patternIdx < Bank<DrumPatternSet>::kPatterns &&
        voiceIdx >= 0 && voiceIdx < DrumPatternSet::kVoices &&
        stepIdx >= 0 && stepIdx < DrumPattern::kSteps &&
        bankIdx >= 0 && bankIdx < kBankCount) {
        DrumStep& step = target_.drumBanks[bankIdx].patterns[patternIdx].voices[voiceIdx].steps[stepIdx];
        if (path == Path::DrumFxArray) step.fx = static_cast<uint8_t>(value);
        else if (path == Path::DrumFxParamArray) step.fxParam = static_cast<uint8_t>(value);
        else step.probability = clampProbability(static_cast<int>(value));
    }
    return;
  }
"""
    new_drum = """  if (path == Path::DrumFxArray || path == Path::DrumFxParamArray ||
      path == Path::DrumProbabilityArray || path == Path::DrumVelocityArray ||
      path == Path::DrumTimingArray) {
    int bankIdx = currentIndexFor(Path::DrumBanks);
    if (bankIdx < 0) bankIdx = 0;
    int patternIdx = currentIndexFor(Path::DrumBank);
    int voiceIdx = currentIndexFor(Path::DrumPatternSet);
    int stepIdx = stack_[stackSize_ - 1].index;
    if (patternIdx >= 0 && patternIdx < Bank<DrumPatternSet>::kPatterns &&
        voiceIdx >= 0 && voiceIdx < DrumPatternSet::kVoices &&
        stepIdx >= 0 && stepIdx < DrumPattern::kSteps &&
        bankIdx >= 0 && bankIdx < kBankCount) {
      DrumStep& step = target_.drumBanks[bankIdx].patterns[patternIdx].voices[voiceIdx].steps[stepIdx];
      const int intValue = static_cast<int>(value);
      if (path == Path::DrumFxArray) {
        step.fx = static_cast<uint8_t>(intValue);
      } else if (path == Path::DrumFxParamArray) {
        step.fxParam = static_cast<uint8_t>(intValue);
      } else if (path == Path::DrumProbabilityArray) {
        step.probability = clampProbability(intValue);
      } else if (path == Path::DrumVelocityArray) {
        int velocity = intValue;
        if (velocity < 0) velocity = 0;
        if (velocity > 127) velocity = 127;
        step.velocity = static_cast<uint8_t>(velocity);
      } else {
        int timing = intValue;
        if (timing < -23) timing = -23;
        if (timing > 23) timing = 23;
        step.timing = static_cast<int8_t>(timing);
      }
    }
    return;
  }
"""
    text = replace_once(text, old_drum, new_drum, "drum dynamic parser")

    text = replace_once(
        text,
        """    } else if (lastKey_ == "prb") {
      pattern.steps[stepIdx].probability = clampProbability(static_cast<int>(value));
""",
        """    } else if (lastKey_ == "vel") {
      int velocity = static_cast<int>(value);
      if (velocity < 0) velocity = 0;
      if (velocity > 127) velocity = 127;
      pattern.steps[stepIdx].velocity = static_cast<uint8_t>(velocity);
    } else if (lastKey_ == "tim") {
      int timing = static_cast<int>(value);
      if (timing < -23) timing = -23;
      if (timing > 23) timing = 23;
      pattern.steps[stepIdx].timing = static_cast<int8_t>(timing);
    } else if (lastKey_ == "prb") {
      pattern.steps[stepIdx].probability = clampProbability(static_cast<int>(value));
""",
        "synth numeric dynamics parser",
    )

    text = replace_once(
        text,
        """    } else if (lastKey_ == "accent") {
      pattern.steps[stepIdx].accent = value;
    }
""",
        """    } else if (lastKey_ == "accent") {
      pattern.steps[stepIdx].accent = value;
    } else if (lastKey_ == "ghost") {
      pattern.steps[stepIdx].ghost = value;
    }
""",
        "synth ghost parser",
    )

    SOURCE.write_text(text, encoding="utf-8")


if __name__ == "__main__":
    main()
