#include "mode_manager.h"
#include "miniacid_engine.h"

namespace {

struct LegacyGenreTimbre {
    float osc;
    float cutoff;
    float resonance;
    float envAmount;
    float envDecay;
};

// Exact base timbre values from the last GenreSceneView::applyGenreTimbre()
// implementation before physical synth ownership was removed from GenreManager.
constexpr LegacyGenreTimbre kLegacyAcidTimbre{
    0.0f, 0.55f, 0.35f, 0.85f, 0.35f};
constexpr LegacyGenreTimbre kLegacyDetroitTechnoTimbre{
    0.2f, 0.60f, 0.30f, 0.75f, 0.20f};

float clamp01(float value) {
    return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

bool supportsLegacyGenreSound(GenerativeMode genre) {
    return genre == GenerativeMode::Acid || genre == GenerativeMode::Techno;
}

}  // namespace

bool GrooveboxModeManager::applyLegacyGenreTimbre(
        GenerativeMode genre, int voiceIndex) {
    if (voiceIndex < 0 || voiceIndex > 1) return false;
    if (engine_.currentSynthEngineName(voiceIndex) != "TB303") return false;

    const LegacyGenreTimbre* timbre = nullptr;
    switch (genre) {
        case GenerativeMode::Acid:
            timbre = &kLegacyAcidTimbre;
            break;
        case GenerativeMode::Techno:
            // Techno was appended after the old Genre timbre projector was
            // removed. Its legacy target is the old Electro/Detroit behavior.
            timbre = &kLegacyDetroitTechnoTimbre;
            break;
        default:
            return false;
    }

    // Historical truth: the old Genre projector attempted to write Oscillator
    // through MiniAcid::set303ParameterNormalized(). TB303's generic uint8_t
    // parameter seam handled only Cutoff/Resonance/EnvAmount/EnvDecay (0..3),
    // so Oscillator(index 4) was an audible no-op. Preserve that actual behavior
    // rather than "fixing" the old intent during restoration.
    (void)timbre->osc;

    float cutoff = timbre->cutoff;
    float resonance = timbre->resonance;
    float envAmount = timbre->envAmount;
    float envDecay = timbre->envDecay;

    // Preserve the historical per-voice clamps exactly, but keep the operation
    // explicit and sound-only instead of restoring GenreManager ownership.
    if (voiceIndex == 0) {
        cutoff = clamp01(cutoff);
        resonance = clamp01(resonance);
        envAmount = clamp01(envAmount);
        envDecay = clamp01(envDecay);

        if (cutoff < 0.18f) cutoff = 0.18f;
        if (cutoff > 0.62f) cutoff = 0.62f;
        if (envAmount < 0.18f) envAmount = 0.18f;
        if (envAmount > 0.55f) envAmount = 0.55f;
        if (envDecay < 0.10f) envDecay = 0.10f;
        if (envDecay > 0.45f) envDecay = 0.45f;
        if (resonance < 0.0f) resonance = 0.0f;
        if (resonance > 0.85f) resonance = 0.85f;
    } else {
        if (cutoff < 0.40f) cutoff = 0.40f;
        if (envAmount < 0.20f) envAmount = 0.20f;
        if (envDecay < 0.08f) envDecay = 0.08f;
        if (cutoff > 0.95f) cutoff = 0.95f;
        if (resonance > 0.95f) resonance = 0.95f;
    }

    engine_.set303ParameterNormalized(
        TB303ParamId::Cutoff, clamp01(cutoff), voiceIndex);
    engine_.set303ParameterNormalized(
        TB303ParamId::Resonance, clamp01(resonance), voiceIndex);
    engine_.set303ParameterNormalized(
        TB303ParamId::EnvAmount, clamp01(envAmount), voiceIndex);
    engine_.set303ParameterNormalized(
        TB303ParamId::EnvDecay, clamp01(envDecay), voiceIndex);
    return true;
}

bool GrooveboxModeManager::setLegacyGenreSoundEnabled(
        bool enabled, GenerativeMode genre) {
    if (!enabled) {
        if (!legacyGenreSoundEnabled_) return true;

        const TB303ParamId ids[4] = {
            TB303ParamId::Cutoff,
            TB303ParamId::Resonance,
            TB303ParamId::EnvAmount,
            TB303ParamId::EnvDecay,
        };

        for (int voice = 0; voice < 2; ++voice) {
            LegacyTimbreSnapshot& snapshot = legacyTimbreSnapshot_[voice];
            if (!snapshot.valid) continue;

            // If the voice engine changed while the transient override was on,
            // do not write a stale TB303 snapshot into a different engine.
            if (engine_.currentSynthEngineName(voice) == "TB303") {
                const float values[4] = {
                    snapshot.cutoff,
                    snapshot.resonance,
                    snapshot.envAmount,
                    snapshot.envDecay,
                };
                for (int param = 0; param < 4; ++param) {
                    engine_.set303ParameterNormalized(ids[param], values[param], voice);
                }
            }
            snapshot = LegacyTimbreSnapshot{};
        }

        legacyGenreSoundEnabled_ = false;
        return true;
    }

    if (!supportsLegacyGenreSound(genre)) return false;

    if (legacyGenreSoundEnabled_) {
        if (legacyGenreSoundGenre_ == genre) return true;
        if (!setLegacyGenreSoundEnabled(false, legacyGenreSoundGenre_)) return false;
    }

    bool appliedAny = false;
    for (int voice = 0; voice < 2; ++voice) {
        LegacyTimbreSnapshot& snapshot = legacyTimbreSnapshot_[voice];
        snapshot = LegacyTimbreSnapshot{};
        if (engine_.currentSynthEngineName(voice) != "TB303") continue;

        snapshot.valid = true;
        snapshot.cutoff =
            engine_.parameter303(TB303ParamId::Cutoff, voice).normalized();
        snapshot.resonance =
            engine_.parameter303(TB303ParamId::Resonance, voice).normalized();
        snapshot.envAmount =
            engine_.parameter303(TB303ParamId::EnvAmount, voice).normalized();
        snapshot.envDecay =
            engine_.parameter303(TB303ParamId::EnvDecay, voice).normalized();

        if (applyLegacyGenreTimbre(genre, voice)) {
            appliedAny = true;
        } else {
            snapshot = LegacyTimbreSnapshot{};
        }
    }

    if (!appliedAny) {
        for (auto& snapshot : legacyTimbreSnapshot_) {
            snapshot = LegacyTimbreSnapshot{};
        }
        return false;
    }

    legacyGenreSoundGenre_ = genre;
    legacyGenreSoundEnabled_ = true;
    return true;
}

bool GrooveboxModeManager::toggleLegacyGenreSound(GenerativeMode genre) {
    if (legacyGenreSoundEnabled_) {
        return setLegacyGenreSoundEnabled(false, legacyGenreSoundGenre_);
    }
    return setLegacyGenreSoundEnabled(true, genre);
}
