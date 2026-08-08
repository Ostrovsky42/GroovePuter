from pathlib import Path

path = Path("src/generation/rhythm/rhythm_realizer.cpp")
text = path.read_text(encoding="utf-8")
old = """    if (!added) {
      // Last bounded option: create a new shared coordinate in legal space.
      int bestBar = -1;
      int bestStep = -1;
      uint32_t bestRank = 0;
      for (uint8_t bar = 0; bar < occupancy.barCount; ++bar) {
        for (uint8_t step = 0; step < kStepsPerBar; ++step) {
          const StepMask bit = stepBit(step);
          if (!(relation.zoneMask & bit) ||
              !isOnsetLegal(archetype, *sourceLane, step) ||
              !isOnsetLegal(archetype, *targetLane, step)) {
            continue;
          }
          const uint32_t rank = deterministicValue(
              seed, candidateCoordinate(bar, relation.target, step));
          if (bestBar < 0 || rank > bestRank) {
            bestBar = bar;
            bestStep = step;
            bestRank = rank;
          }
        }
      }
      if (bestBar >= 0 &&
          addStructuralCandidate(archetype, *sourceLane, occupancy,
                                 static_cast<uint8_t>(bestBar),
                                 static_cast<uint8_t>(bestStep)) &&
          addStructuralCandidate(archetype, *targetLane, occupancy,
                                 static_cast<uint8_t>(bestBar),
                                 static_cast<uint8_t>(bestStep))) {
        added = true;
      }
    }
"""
new = """    if (!added) {
      // Last bounded option: create a new shared coordinate in legal space.
      // Probe each pair transactionally so a source addition whose matching
      // target conflicts with another hard relationship cannot poison the
      // search or hide a different feasible Coincide coordinate.
      bool foundPair = false;
      uint32_t bestRank = 0;
      PhraseOccupancy bestOccupancy{};
      for (uint8_t bar = 0; bar < occupancy.barCount; ++bar) {
        for (uint8_t step = 0; step < kStepsPerBar; ++step) {
          const StepMask bit = stepBit(step);
          if (!(relation.zoneMask & bit) ||
              !isOnsetLegal(archetype, *sourceLane, step) ||
              !isOnsetLegal(archetype, *targetLane, step)) {
            continue;
          }

          PhraseOccupancy trial = occupancy;
          if (!addStructuralCandidate(archetype, *sourceLane, trial,
                                      bar, step) ||
              !addStructuralCandidate(archetype, *targetLane, trial,
                                      bar, step)) {
            continue;
          }

          const uint32_t rank = deterministicValue(
              seed, candidateCoordinate(bar, relation.target, step));
          if (!foundPair || rank > bestRank) {
            foundPair = true;
            bestRank = rank;
            bestOccupancy = trial;
          }
        }
      }
      if (foundPair) {
        occupancy = bestOccupancy;
        added = true;
      }
    }
"""
if text.count(old) != 1:
    raise SystemExit(f"expected one Coincide fallback target, found {text.count(old)}")
path.write_text(text.replace(old, new, 1), encoding="utf-8")
