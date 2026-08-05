#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def replace_once(relative_path: str, old: str, new: str) -> bool:
    path = ROOT / relative_path
    text = path.read_text(encoding="utf-8")
    if new in text:
        return False
    count = text.count(old)
    if count != 1:
        raise RuntimeError(
            f"{relative_path}: expected one replacement target, found {count}"
        )
    path.write_text(text.replace(old, new, 1), encoding="utf-8")
    return True


changed = False

changed |= replace_once(
    "src/dsp/mode_manager.cpp",
    """    int baseRoot;
    int octaveRange;
    
    if (isBass) {
        // Bass: low register, narrow range
        baseRoot = 24; // C1
        if (params.minOctave > 0 && params.minOctave < 36) baseRoot = params.minOctave;
        octaveRange = 1; // Stay within 1 octave
    } else {
""",
    """    int baseRoot;
    int octaveRange;
    int bassMaxNote = 127;
    
    if (isBass) {
        // minOctave/maxOctave are MIDI-note bounds despite their legacy names.
        // Respect the genre floor instead of forcing every bass line to C1.
        baseRoot = std::max(0, std::min(params.minOctave, 127));
        const int requestedMax = std::max(baseRoot, std::min(params.maxOctave, 127));
        bassMaxNote = std::min(requestedMax, baseRoot + 12);
        octaveRange = 1; // Bass remains within one octave above its genre floor.
    } else {
""",
)

changed |= replace_once(
    "src/dsp/mode_manager.cpp",
    """            if (!isBass && behavior.forceOctaveJump && (boundedRandom(rng, 100) < 30)) note += 12;
            if (behavior.allowChromatic && (boundedRandom(rng, 100) < 20)) note += (boundedRandom(rng, 3)) - 1;

            motif[i] = note;
""",
    """            if (!isBass && behavior.forceOctaveJump && (boundedRandom(rng, 100) < 30)) note += 12;
            if (behavior.allowChromatic && (boundedRandom(rng, 100) < 20)) note += (boundedRandom(rng, 3)) - 1;
            if (isBass) note = std::max(baseRoot, std::min(note, bassMaxNote));

            motif[i] = note;
""",
)

changed |= replace_once(
    "src/dsp/mode_manager.cpp",
    """        pattern.steps[step].note = note;
        lastNote = note;
""",
    """        if (isBass) note = std::max(baseRoot, std::min(note, bassMaxNote));
        pattern.steps[step].note = note;
        lastNote = note;
""",
)

changed |= replace_once(
    "src/dsp/genre_manager.cpp",
    """            // Force low-ish ranges (Soft Clamps)
            if (cut < 0.05f) cut = 0.05f;
            if (cut > 0.45f) cut = 0.45f;

            if (env < 0.02f) env = 0.02f;
            if (env > 0.20f) env = 0.20f;

            if (decay < 0.04f) decay = 0.04f;
            if (decay > 0.25f) decay = 0.25f;
""",
    """            // Keep the bass darker than Synth B without erasing the
            // genre timbre. The previous caps made many profiles nearly mute.
            if (cut < 0.18f) cut = 0.18f;
            if (cut > 0.62f) cut = 0.62f;

            if (env < 0.18f) env = 0.18f;
            if (env > 0.55f) env = 0.55f;

            if (decay < 0.10f) decay = 0.10f;
            if (decay > 0.45f) decay = 0.45f;
""",
)

changed |= replace_once(
    "tests/run_host_tests.sh",
    'python3 "${ROOT_DIR}/tests/test_generation_rng_source_regressions.py"\n',
    'python3 "${ROOT_DIR}/tests/test_generation_rng_source_regressions.py"\n'
    'python3 "${ROOT_DIR}/tests/test_synth_a_bass_profile_source_regressions.py"\n',
)

print("Synth A bass audibility patch applied" if changed else "Patch already applied")
