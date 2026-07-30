#!/usr/bin/env python3
"""Apply the P0 genre-generator reliability repair.

Every source replacement is assertion-guarded. Source drift aborts before a
commit can be created.
"""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(
            f"{path}: expected exactly one occurrence, found {count}\n{old[:160]}"
        )
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


def patch_safe_defaults() -> None:
    path = ROOT / "src/dsp/genre_manager.h"
    old = """struct GenerativeParams {
    // Pattern density
    int minNotes;
    int maxNotes;
    
    // Note range
    int minOctave;  // MIDI note for lowest octave
    int maxOctave;  // MIDI note for highest octave
    
    // Articulation
    float slideProbability;     // 0-1
    float accentProbability;    // 0-1
    float gateLengthMultiplier; // 0.1-1.0
    
    // Timing
    float swingAmount;          // 0-0.66
    float microTimingAmount;    // 0-1 human feel
    
    // Velocity
    int velocityMin;
    int velocityMax;
    
    // Structure
    bool preferDownbeats;
    bool allowRepeats;
    float rootNoteBias;         // 0-1, probability of root
    float ghostProbability;     // 0-1
    float chromaticProbability; // 0-1
    
    // Drum settings
    bool sparseKick;
    bool sparseHats;
    bool noAccents;
    float fillProbability;

    // Drum groove (fields only — preset values filled by user)
    float drumSyncopation = 0.0f;     // 0-1, syncopation amount
    bool  drumPreferOffbeat = false;   // prefer offbeat hat placement
    int   drumVoiceCount = 8;          // active voices (1-8)
};
"""
    new = """struct GenerativeParams {
    // Safe neutral defaults are required because this type is also used by
    // compatibility adapters. No default construction may expose stack data to
    // note, timing, articulation or drum probability generation.

    // Pattern density
    int minNotes = 4;
    int maxNotes = 8;

    // Note range (MIDI notes)
    int minOctave = 36;
    int maxOctave = 60;

    // Articulation
    float slideProbability = 0.10f;
    float accentProbability = 0.25f;
    float gateLengthMultiplier = 0.50f;

    // Timing
    float swingAmount = 0.0f;
    float microTimingAmount = 0.0f;

    // Velocity
    int velocityMin = 80;
    int velocityMax = 110;

    // Structure
    bool preferDownbeats = true;
    bool allowRepeats = true;
    float rootNoteBias = 0.40f;
    float ghostProbability = 0.05f;
    float chromaticProbability = 0.0f;

    // Drum settings
    bool sparseKick = false;
    bool sparseHats = false;
    bool noAccents = false;
    float fillProbability = 0.15f;

    // Drum groove
    float drumSyncopation = 0.0f;
    bool drumPreferOffbeat = false;
    int drumVoiceCount = 8;
};
"""
    replace_once(path, old, new)


def patch_recipe_adapters() -> None:
    path = ROOT / "src/dsp/mode_manager.cpp"
    text = path.read_text(encoding="utf-8")
    marker = "// GROOVE RECIPE OVERLOADS"
    marker_index = text.index(marker)
    prefix = text[:marker_index]
    adapters = text[marker_index:]
    old = "    GenerativeParams params;"
    count = adapters.count(old)
    if count != 3:
        raise RuntimeError(
            f"{path}: expected three legacy recipe adapters, found {count}"
        )
    new = (
        "    GenerativeParams params = "
        "engine_.genreManager().getCompiledGenerativeParams();"
    )
    path.write_text(prefix + adapters.replace(old, new), encoding="utf-8")


def patch_engine_generation_path() -> None:
    path = ROOT / "src/dsp/miniacid_engine.cpp"

    replace_once(
        path,
        """void MiniAcid::randomize303Pattern(int voiceIndex) {
  int idx = clamp303Voice(voiceIndex);
  // Use genre-aware generator with voice role (0=bass, 1=lead)
  const auto recipe = genreManager_.getGrooveRecipe();
  auto behavior = genreManager_.getBehavior();
""",
        """void MiniAcid::randomize303Pattern(int voiceIndex) {
  int idx = clamp303Voice(voiceIndex);
  // Use the complete compiled genre profile. GrooveRecipe is a compact legacy
  // view and cannot represent pitch, articulation or microtiming parameters.
  const GenerativeParams& genreParams =
      genreManager_.getCompiledGenerativeParams();
  auto behavior = genreManager_.getBehavior();
""",
    )
    replace_once(
        path,
        "  modeManager_.generatePattern(editSynthPattern(idx), bpmValue, recipe, behavior, idx);",
        "  modeManager_.generatePattern(editSynthPattern(idx), bpmValue, genreParams, behavior, idx);",
    )

    replace_once(
        path,
        """void MiniAcid::randomizeDrumPattern() {
  // Use genre-aware drum generator
  const auto recipe = genreManager_.getGrooveRecipe();
  const auto behavior = genreManager_.getBehavior();
  modeManager_.generateDrumPattern(sceneManager_.editCurrentDrumPattern(), recipe, behavior);
}
""",
        """void MiniAcid::randomizeDrumPattern() {
  const GenerativeParams& genreParams =
      genreManager_.getCompiledGenerativeParams();
  const auto behavior = genreManager_.getBehavior();
  modeManager_.generateDrumPattern(
      sceneManager_.editCurrentDrumPattern(), genreParams, behavior);
}
""",
    )

    replace_once(
        path,
        """void MiniAcid::randomizeDrumVoice(int voiceIndex) {
  int idx = clampDrumVoice(voiceIndex);
  const auto recipe = genreManager_.getGrooveRecipe();
  const auto behavior = genreManager_.getBehavior();
  modeManager_.generateDrumVoice(sceneManager_.editCurrentDrumPattern().voices[idx], idx, recipe, behavior);
}
""",
        """void MiniAcid::randomizeDrumVoice(int voiceIndex) {
  int idx = clampDrumVoice(voiceIndex);
  const GenerativeParams& genreParams =
      genreManager_.getCompiledGenerativeParams();
  const auto behavior = genreManager_.getBehavior();
  modeManager_.generateDrumVoice(
      sceneManager_.editCurrentDrumPattern().voices[idx], idx,
      genreParams, behavior);
}
""",
    )

    replace_once(
        path,
        """void MiniAcid::randomizeDrumPatternChaos() {
  const auto recipe = genreManager_.getGrooveRecipe();
  
  // Scramble EVERYTHING
""",
        """void MiniAcid::randomizeDrumPatternChaos() {
  const GenerativeParams& genreParams =
      genreManager_.getCompiledGenerativeParams();

  // Scramble structural placement while retaining bounded genre parameters.
""",
    )
    replace_once(
        path,
        "      modeManager_.generateDrumVoice(patternSet.voices[v], v, recipe, chaosBehavior);",
        "      modeManager_.generateDrumVoice(patternSet.voices[v], v, genreParams, chaosBehavior);",
    )

    replace_once(
        path,
        """  const auto recipe = genreManager_.getGrooveRecipe();
  const auto behavior = genreManager_.getBehavior();

  // Regenerate 303 patterns using generative mode + structural behavior
""",
        """  const GenerativeParams& genreParams =
      genreManager_.getCompiledGenerativeParams();
  const auto behavior = genreManager_.getBehavior();

  // Regenerate synth patterns using the complete compiled genre profile.
""",
    )
    replace_once(
        path,
        "  modeManager_.generatePattern(editSynthPattern(0), bpmValue, recipe, bassBehavior, 0); // Bass\n"
        "  modeManager_.generatePattern(editSynthPattern(1), bpmValue, recipe, leadBehavior, 1); // Lead",
        "  modeManager_.generatePattern(\n"
        "      editSynthPattern(0), bpmValue, genreParams, bassBehavior, 0); // Bass\n"
        "  modeManager_.generatePattern(\n"
        "      editSynthPattern(1), bpmValue, genreParams, leadBehavior, 1); // Lead",
    )
    replace_once(
        path,
        "  modeManager_.generateDrumPattern(sceneManager_.editCurrentDrumPattern(), recipe, behavior);",
        "  modeManager_.generateDrumPattern(\n"
        "      sceneManager_.editCurrentDrumPattern(), genreParams, behavior);",
    )

    replace_once(
        path,
        """  const GrooveboxMode linkedMode =
      GenreManager::grooveboxModeForGenerative(genreManager_.generativeMode());
""",
        """  const GrooveboxMode linkedMode =
      GenreManager::grooveboxModeForRecipe(
          genreManager_.recipe(), genreManager_.generativeMode());
""",
    )


def main() -> None:
    patch_safe_defaults()
    patch_recipe_adapters()
    patch_engine_generation_path()


if __name__ == "__main__":
    main()
