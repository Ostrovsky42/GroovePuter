from pathlib import Path

path = Path("src/generation/rhythm/rhythm_realizer.cpp")
text = path.read_text(encoding="utf-8")
old = """      int bestAbsolute = -1;
      uint32_t bestRank = 0;
      const int targetAbsolute =
          targetBar * kStepsPerBar + targetStep;
      for (int offset = relation.minOffset;
           offset <= relation.maxOffset;
           ++offset) {
        const int sourceAbsolute = targetAbsolute - offset;
        if (!coordinateInPhrase(occupancy.barCount, sourceAbsolute)) continue;
        const uint8_t sourceBar = static_cast<uint8_t>(
            sourceAbsolute / kStepsPerBar);
        const uint8_t sourceStep = static_cast<uint8_t>(
            sourceAbsolute % kStepsPerBar);
        if (!relationCoordinateAllowed(relation, sourceBar, sourceStep,
                                       targetBar, targetStep) ||
            !isOnsetLegal(archetype, *sourceLane, sourceStep)) {
          continue;
        }
        const uint32_t rank = deterministicValue(
            seed, candidateCoordinate(sourceBar,
                                      relation.source, sourceStep));
        if (bestAbsolute < 0 || rank > bestRank) {
          bestAbsolute = sourceAbsolute;
          bestRank = rank;
        }
      }

      bool repaired = false;
      if (bestAbsolute >= 0) {
        repaired = addStructuralCandidate(
            archetype, *sourceLane, occupancy,
            static_cast<uint8_t>(bestAbsolute / kStepsPerBar),
            static_cast<uint8_t>(bestAbsolute % kStepsPerBar));
      }
"""
new = """      bool foundSource = false;
      uint32_t bestRank = 0;
      PhraseOccupancy bestOccupancy{};
      const int targetAbsolute =
          targetBar * kStepsPerBar + targetStep;
      for (int offset = relation.minOffset;
           offset <= relation.maxOffset;
           ++offset) {
        const int sourceAbsolute = targetAbsolute - offset;
        if (!coordinateInPhrase(occupancy.barCount, sourceAbsolute)) continue;
        const uint8_t sourceBar = static_cast<uint8_t>(
            sourceAbsolute / kStepsPerBar);
        const uint8_t sourceStep = static_cast<uint8_t>(
            sourceAbsolute % kStepsPerBar);
        if (!relationCoordinateAllowed(relation, sourceBar, sourceStep,
                                       targetBar, targetStep) ||
            !isOnsetLegal(archetype, *sourceLane, sourceStep)) {
          continue;
        }

        PhraseOccupancy trial = occupancy;
        if (!addStructuralCandidate(archetype, *sourceLane, trial,
                                    sourceBar, sourceStep)) {
          continue;
        }
        const uint32_t rank = deterministicValue(
            seed, candidateCoordinate(sourceBar,
                                      relation.source, sourceStep));
        if (!foundSource || rank > bestRank) {
          foundSource = true;
          bestRank = rank;
          bestOccupancy = trial;
        }
      }

      bool repaired = false;
      if (foundSource) {
        occupancy = bestOccupancy;
        repaired = true;
      }
"""
if text.count(old) != 1:
    raise SystemExit(f"expected one Offset repair target, found {text.count(old)}")
path.write_text(text.replace(old, new, 1), encoding="utf-8")
