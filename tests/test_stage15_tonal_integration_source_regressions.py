import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MIGRATION_H = (ROOT / "src/generation/migration/strong_rhythm_migration.h").read_text()
MIGRATION_CPP = (ROOT / "src/generation/migration/strong_rhythm_migration.cpp").read_text()
LIVE = (ROOT / "src/generation/migration/strong_rhythm_live_bridge.cpp").read_text()
PROFILE = (ROOT / "src/generation/composition/tonal_profile.cpp").read_text()
MATERIALIZER = (ROOT / "src/generation/tonal/tonal_materializer.cpp").read_text()
BASS = (ROOT / "src/generation/roles/bass_pitch_behavior.cpp").read_text()
MELODY = (ROOT / "src/generation/roles/melodic_pitch_intent.cpp").read_text()

assert "bool tonalMaterializationEnabled = false" in MIGRATION_H
assert "rootPitchClass" in MIGRATION_H
assert "scaleTypeValue" in MIGRATION_H
assert "context.tonalMaterializationEnabled = true" in LIVE
assert "scene.generatorParams.scaleRoot" in LIVE
assert "scene.generatorParams.scale" in LIVE

# Legacy rollback path stays explicit; live production opts into the new owner.
assert "if (!context.tonalMaterializationEnabled)" in MIGRATION_CPP
assert "projectLegacyPitchPattern(" in MIGRATION_CPP
assert "materializeTonalIntent(" in MIGRATION_CPP
assert "result.tonalMaterializationApplied = true" in MIGRATION_CPP

# Roles remain semantic intent producers and never call the projector/materializer.
for role_text in (BASS, MELODY):
    assert "projectTonalIntent(" not in role_text
    assert "materializeTonalIntent(" not in role_text
    assert "SynthPattern" not in role_text
    assert "Scene" not in role_text

# Tonal policy is data-driven. No mode switch may leak into roles/tonal/profile.
assert "switch (mode" not in PROFILE
assert "switch (settings.generativeMode" not in PROFILE
assert "GenerativeMode::Acid" in PROFILE
assert "GenerativeMode::Techno" in PROFILE
assert "GenerativeMode::LoFi" in PROFILE
assert "tonalGenerationProfileFor(settings)" in MIGRATION_CPP

# ChordRhythm remains harmonic-time owner. Materializer consumes its event mask;
# it does not call any rhythm generator.
assert "harmonicEventOnsets" in MATERIALIZER
for forbidden in ("realizeChordRhythm", "realizeBassRhythm", "realizeMelodicMotif"):
    assert forbidden not in MATERIALIZER

# No new RNG domain or floating-point generation logic in the tonal integration.
RHYTHM_TYPES = (ROOT / "src/generation/rhythm/rhythm_types.h").read_text()
assert "BassPitch" in RHYTHM_TYPES
assert "LeadPitch" in RHYTHM_TYPES
assert "ChordPitch" in RHYTHM_TYPES
for path in (
    ROOT / "src/generation/tonal/tonal_materializer.cpp",
    ROOT / "src/generation/composition/tonal_profile.cpp",
):
    code = path.read_text()
    assert "rand(" not in code
    assert "random_device" not in code
    assert "new " not in code
    assert "malloc(" not in code
    assert re.search(r"\bfloat\b", code) is None
    assert re.search(r"\bdouble\b", code) is None

print("Stage 15 tonal integration source regressions: OK")
