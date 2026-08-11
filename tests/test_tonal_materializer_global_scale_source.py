from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src/generation/tonal/tonal_materializer.cpp").read_text()

# Event-local register placement must preserve the Scene/global scale. The role
# degree is resolved in the global scale and converted to an exact relative
# semitone displacement around the harmonic event root.
assert "const int targetDegree = static_cast<int>(event.degree) +" in SOURCE
assert "scaleDegreeToSemitone(request.scaleTypeValue, targetDegree) -" in SOURCE
assert "eventScaleSemitone" in SOURCE
assert "projection.semitoneOffsetOrdinals = static_cast<uint16_t>(" in SOURCE

# Regression guard: do not pass raw untagged role degree offsets into a
# TonalProjector request rooted at each harmonic event, which would transpose
# the ScaleType itself at every chord root.
assert "projection.tonalOffsets[projectedOrdinal] =\n        request.tonalOffsets[ordinal]" not in SOURCE

print("Stage 15 global-scale materializer source regression: OK")
