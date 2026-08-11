from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src/generation/tonal/tonal_materializer.cpp").read_text()

# Harmonic events may change the local harmonic target, but they must never
# transpose the Scene/global ScaleType. TonalMaterializer resolves every role
# degree through request.scaleTypeValue and passes exact semitone displacements
# to the shared TonalProjector.
assert "projection.scaleTypeValue = request.scaleTypeValue;" in SOURCE
assert "const HarmonicEvent event = harmonicEventForStep(" in SOURCE
assert "scaleDegreeToSemitone(request.scaleTypeValue, event.degree) +" in SOURCE
assert "static_cast<int>(event.degree) +" in SOURCE
assert "static_cast<int>(request.tonalOffsets[ordinal])) +" in SOURCE
assert "static_cast<int>(event.rootOffsetSemitones)" in SOURCE
assert "projection.semitoneOffsetOrdinals = ordinalMask(onsetCount);" in SOURCE

# Regression guards: do not derive a new/transposed scale from a harmonic event,
# and do not pass role degree offsets to TonalProjector as untagged scale degrees.
assert "projection.scaleTypeValue = event" not in SOURCE
assert "projection.rootPitchClass = event" not in SOURCE
assert "projection.semitoneOffsetOrdinals = 0" not in SOURCE

print("Stage 15 global-scale materializer source regression: OK")
