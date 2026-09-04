#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATH = ROOT / "src/dsp/miniacid_engine.cpp"
text = PATH.read_text(encoding="utf-8")

old = '''  const int bankIndex = current303BankIndex(synthIndex);
  const int patternIndex = current303PatternIndex(synthIndex);
  if (bankIndex < 0 || bankIndex >= kBankCount ||
      patternIndex < 0 || patternIndex >= Bank<SynthPattern>::kPatterns) {
    return patternRuntimeBank_.empty();
  }
'''
new = '''  // Keep the accepted SONG/PATTERN source gate. In SONG mode an empty synth
  // track is authoritative silence; it must not fall back to the Scene's
  // current PATTERN-mode index merely because that resident slot exists.
  const SongTrack track = synthIndex == 0 ? SongTrack::SynthA : SongTrack::SynthB;
  const int patternIndex = songPatternIndexForTrack(track);
  if (patternIndex < 0) return patternRuntimeBank_.empty();
  const int bankIndex = current303BankIndex(synthIndex);
  if (bankIndex < 0 || bankIndex >= kBankCount ||
      patternIndex >= Bank<SynthPattern>::kPatterns) {
    return patternRuntimeBank_.empty();
  }
'''
count = text.count(old)
if count != 1:
    raise RuntimeError(f"expected one runtime selector address block, got {count}")
PATH.write_text(text.replace(old, new, 1), encoding="utf-8")
print("P2 SONG/PATTERN runtime source gating applied")
