from pathlib import Path

path = Path("src/generation/rhythm/rhythm_realizer.cpp")
text = path.read_text(encoding="utf-8")


def replace_once(old: str, new: str) -> None:
    global text
    if text.count(old) != 1:
        raise SystemExit(f"expected exactly one patch target, found {text.count(old)}")
    text = text.replace(old, new, 1)


replace_once(
    """const BarTrajectory* chooseTrajectory(const RhythmCatalogView& catalog,
                                      const RhythmArchetype& archetype,
                                      uint8_t phraseBars,
                                      RealizationLevel level,
                                      TrajectoryId pinned,
                                      const GenerationContext& context) {
""",
    """bool trajectorySupportsIntent(const BarTrajectory& trajectory,
                              TransformationIntent intent) {
  if (intent == TransformationIntent::Auto ||
      intent == TransformationIntent::Fill) {
    return true;
  }

  BarFunction required = BarFunction::Statement;
  switch (intent) {
    case TransformationIntent::Reduce:
      required = BarFunction::Reduction;
      break;
    case TransformationIntent::Break:
      required = BarFunction::Break;
      break;
    case TransformationIntent::Build:
      required = BarFunction::Build;
      break;
    case TransformationIntent::Turnaround:
      required = BarFunction::Turnaround;
      break;
    case TransformationIntent::Response:
      required = BarFunction::Response;
      break;
    case TransformationIntent::Auto:
    case TransformationIntent::Fill:
    case TransformationIntent::Count:
      return true;
  }

  for (uint8_t bar = 0; bar < trajectory.barCount; ++bar) {
    if (trajectory.bars[bar] == required) return true;
  }
  return false;
}

const BarTrajectory* chooseTrajectory(const RhythmCatalogView& catalog,
                                      const RhythmArchetype& archetype,
                                      uint8_t phraseBars,
                                      RealizationLevel level,
                                      TransformationIntent requestedIntent,
                                      TrajectoryId pinned,
                                      const GenerationContext& context) {
""",
)

replace_once(
    """    if (!ref || !trajectory || trajectory->barCount != phraseBars ||
        !(ref->allowedLevels & realizationLevelBit(level))) {
""",
    """    if (!ref || !trajectory || trajectory->barCount != phraseBars ||
        !(ref->allowedLevels & realizationLevelBit(level)) ||
        !trajectorySupportsIntent(*trajectory, requestedIntent)) {
""",
)

replace_once(
    """    if (!trajectory || trajectory->barCount != phraseBars ||
        !(ref.allowedLevels & realizationLevelBit(level))) {
      continue;
    }
    totalWeight = static_cast<uint16_t>(totalWeight + ref.weight);
""",
    """    if (!trajectory || trajectory->barCount != phraseBars ||
        !(ref.allowedLevels & realizationLevelBit(level)) ||
        !trajectorySupportsIntent(*trajectory, requestedIntent)) {
      continue;
    }
    totalWeight = static_cast<uint16_t>(totalWeight + ref.weight);
""",
)

replace_once(
    """    if (!trajectory || trajectory->barCount != phraseBars ||
        !(ref.allowedLevels & realizationLevelBit(level))) {
      continue;
    }
    if (pick < ref.weight) return trajectory;
""",
    """    if (!trajectory || trajectory->barCount != phraseBars ||
        !(ref.allowedLevels & realizationLevelBit(level)) ||
        !trajectorySupportsIntent(*trajectory, requestedIntent)) {
      continue;
    }
    if (pick < ref.weight) return trajectory;
""",
)

replace_once(
    """  const BarTrajectory* trajectory = chooseTrajectory(
      *request.catalog, *archetype, request.phraseBars, request.level,
      pinned, request.generation);
""",
    """  const BarTrajectory* trajectory = chooseTrajectory(
      *request.catalog, *archetype, request.phraseBars, request.level,
      request.intent, pinned, request.generation);
""",
)

path.write_text(text, encoding="utf-8")
