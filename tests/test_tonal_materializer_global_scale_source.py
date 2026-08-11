from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src/generation/tonal/tonal_materializer.cpp").read_text()

# Harmonic events choose their own local root/register anchor, but degree
# arithmetic must remain in the one Scene/global ScaleType. The materializer
# resolves an exact semitone displacement relative to each event root before
# calling the shared projector.
assert "projection.scaleTypeValue = request.scaleTypeValue;" in SOURCE
assert "const int eventSemitone = eventSemitoneFromGlobalRoot(request, event);" in SOURCE
assert "projection.rootPitchClass = normalizePitchClass(" in SOURCE
assert "static_cast<int>(request.rootPitchClass) + eventSemitone" in SOURCE
assert "targetSemitoneFromGlobalRoot(request, event, globalOrdinal)" in SOURCE
assert "const int relativeSemitone = targetSemitone - eventSemitone;" in SOURCE
assert "projection.semitoneOffsetOrdinals = ordinalMask(localOrdinal);" in SOURCE
assert "for (uint8_t eventIndex = 0; eventIndex < eventCount; ++eventIndex)" in SOURCE
assert "projectTonalIntent(projectionRequest)" in SOURCE

# Regression guards: do not transpose ScaleType per chord event, do not return
# to one Scene-root anchor for the entire phrase, and do not pass event-local
# role degrees to TonalProjector as untagged scale-degree intent.
assert "projection.scaleTypeValue = event" not in SOURCE
assert "projection.rootPitchClass = request.rootPitchClass;" not in SOURCE
assert "projection.semitoneOffsetOrdinals = 0" not in SOURCE

print("Stage 15 global-scale/event-local materializer source regression: OK")
