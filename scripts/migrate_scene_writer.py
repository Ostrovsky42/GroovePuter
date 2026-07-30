#!/usr/bin/env python3
"""One-time guarded migration for the streaming scene writer.

The script intentionally fails unless every expected source fragment occurs
exactly once. It is removed by the CI migration commit after successful use.
"""

from pathlib import Path


SCENES_HEADER = Path("scenes.h")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected 1 occurrence, found {count}")
    return text.replace(old, new, 1)


def main() -> None:
    text = SCENES_HEADER.read_text(encoding="utf-8")

    start = text.index(
        "  auto writeString = [&](const std::string& value) -> bool {"
    )
    end = text.index("  auto writeDrumPattern =", start)
    write_string = r'''  auto writeString = [&](const std::string& value) -> bool {
    if (!writeChar('"')) return false;
    for (char ch : value) {
      const unsigned char byte = static_cast<unsigned char>(ch);
      switch (ch) {
        case '"': if (!writeLiteral("\\\"")) return false; break;
        case '\\': if (!writeLiteral("\\\\")) return false; break;
        case '\b': if (!writeLiteral("\\b")) return false; break;
        case '\f': if (!writeLiteral("\\f")) return false; break;
        case '\n': if (!writeLiteral("\\n")) return false; break;
        case '\r': if (!writeLiteral("\\r")) return false; break;
        case '\t': if (!writeLiteral("\\t")) return false; break;
        default:
          if (byte < 0x20) {
            char escaped[7];
            int written = std::snprintf(
                escaped, sizeof(escaped), "\\u%04x", byte);
            if (written != 6 || !writeChunk(escaped, 6)) return false;
          } else if (!writeChar(ch)) {
            return false;
          }
          break;
      }
    }
    return writeChar('"');
  };
'''
    text = text[:start] + write_string + text[end:]

    drum_fx = '    if (!writeLiteral("],\\"fx\\":[")) return false;\n'
    drum_dynamics = '''    if (!writeLiteral("],\\"vel\\":[")) return false;
    for (int i = 0; i < DrumPattern::kSteps; ++i) {
      if (i > 0 && !writeChar(',')) return false;
      if (!writeInt(pattern.steps[i].velocity)) return false;
    }
    if (!writeLiteral("],\\"tim\\":[")) return false;
    for (int i = 0; i < DrumPattern::kSteps; ++i) {
      if (i > 0 && !writeChar(',')) return false;
      if (!writeInt(pattern.steps[i].timing)) return false;
    }
''' + drum_fx
    text = replace_once(text, drum_fx, drum_dynamics, "drum dynamics")

    synth_accent = '''      if (!writeLiteral(",\\"accent\\":")) return false;
      if (!writeBool(pattern.steps[i].accent)) return false;
'''
    synth_dynamics = synth_accent + '''      if (!writeLiteral(",\\"ghost\\":")) return false;
      if (!writeBool(pattern.steps[i].ghost)) return false;
      if (!writeLiteral(",\\"vel\\":")) return false;
      if (!writeInt(pattern.steps[i].velocity)) return false;
      if (!writeLiteral(",\\"tim\\":")) return false;
      if (!writeInt(pattern.steps[i].timing)) return false;
'''
    text = replace_once(text, synth_accent, synth_dynamics, "synth dynamics")

    feel_bars = '''  if (!writeInt(scene_->feel.patternBars)) return false;
  if (!writeLiteral(",\\"lofi\\":")) return false;
'''
    feel_swing = '''  if (!writeInt(scene_->feel.patternBars)) return false;
  if (!writeLiteral(",\\"swing\\":")) return false;
  if (!writeInt(scene_->feel.swingPct)) return false;
  if (!writeLiteral(",\\"mask\\":")) return false;
  if (!writeInt(scene_->feel.swingMask)) return false;
  if (!writeLiteral(",\\"lofi\\":")) return false;
'''
    text = replace_once(text, feel_bars, feel_swing, "feel swing")

    track_volumes = '  if (!writeLiteral(",\\"trackVolumes\\":[")) return false;\n'
    generator = '''  if (!writeLiteral(",\\"generatorParams\\":{\\"minNotes\\":")) return false;
  if (!writeInt(scene_->generatorParams.minNotes)) return false;
  if (!writeLiteral(",\\"maxNotes\\":")) return false;
  if (!writeInt(scene_->generatorParams.maxNotes)) return false;
  if (!writeLiteral(",\\"minOctave\\":")) return false;
  if (!writeInt(scene_->generatorParams.minOctave)) return false;
  if (!writeLiteral(",\\"maxOctave\\":")) return false;
  if (!writeInt(scene_->generatorParams.maxOctave)) return false;
  if (!writeLiteral(",\\"swingAmount\\":")) return false;
  if (!writeFloat(scene_->generatorParams.swingAmount)) return false;
  if (!writeLiteral(",\\"velocityRange\\":")) return false;
  if (!writeFloat(scene_->generatorParams.velocityRange)) return false;
  if (!writeLiteral(",\\"ghostNoteProbability\\":")) return false;
  if (!writeFloat(scene_->generatorParams.ghostNoteProbability)) return false;
  if (!writeLiteral(",\\"microTimingAmount\\":")) return false;
  if (!writeFloat(scene_->generatorParams.microTimingAmount)) return false;
  if (!writeLiteral(",\\"preferDownbeats\\":")) return false;
  if (!writeBool(scene_->generatorParams.preferDownbeats)) return false;
  if (!writeLiteral(",\\"scaleQuantize\\":")) return false;
  if (!writeBool(scene_->generatorParams.scaleQuantize)) return false;
  if (!writeLiteral(",\\"scaleRoot\\":")) return false;
  if (!writeInt(scene_->generatorParams.scaleRoot)) return false;
  if (!writeLiteral(",\\"scale\\":")) return false;
  if (!writeInt(static_cast<int>(scene_->generatorParams.scale))) return false;
  if (!writeChar('}')) return false;

''' + track_volumes
    text = replace_once(
        text, track_volumes, generator, "generator parameters"
    )

    SCENES_HEADER.write_text(text, encoding="utf-8")


if __name__ == "__main__":
    main()
