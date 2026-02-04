#include "genre_manager.h"
#include "miniacid_engine.h"

// ============================================================================
// PRESET TABLE DEFINITIONS (single copy, in flash)
// Order must match struct field order exactly for C++17 compatibility
// ============================================================================

// GenerativeParams fields in order:
// minNotes, maxNotes, minOctave, maxOctave,
// slideProbability, accentProbability, gateLengthMultiplier,
// swingAmount, microTimingAmount,
// velocityMin, velocityMax,
// preferDownbeats, allowRepeats, rootNoteBias, ghostProbability, chromaticProbability,
// sparseKick, sparseHats, noAccents, fillProbability

const GenerativeParams kGenerativePresets[kGenerativeModeCount] = {
    // ACID - Melodic, slides, aggressive
    { 8, 14, 36, 72,  0.40f, 0.50f, 0.8f,  0.0f, 0.1f,  85, 120,  false, true, 0.25f, 0.10f, 0.15f,  false, false, false, 0.6f },
    // OUTRUN - Bright lead/arp, 80s synthwave (swing LOW for drive)
    { 10, 14, 48, 72,  0.12f, 0.25f, 0.70f,  0.08f, 0.01f,  90, 118,  false, true, 0.10f, 0.03f, 0.05f,  false, false, false, 0.4f },
    // DARKSYNTH - Evil bass (swing ZERO, repeats ON for bass anchor)
    { 4, 7, 24, 48,  0.05f, 0.50f, 0.35f,  0.0f, 0.0f,  100, 125,  true, true, 0.70f, 0.0f, 0.03f,  false, true, false, 0.25f },
    // ELECTRO - Staccato, syncopated, mechanical
    { 6, 10, 36, 60,  0.0f, 0.70f, 0.3f,  0.0f, 0.0f,  105, 115,  false, true, 0.30f, 0.05f, 0.10f,  false, false, false, 0.5f },
    // RAVE - Dense, high energy
    { 12, 16, 36, 72,  0.20f, 0.80f, 0.5f,  0.0f, 0.0f,  110, 127,  false, true, 0.20f, 0.05f, 0.20f,  false, false, false, 0.7f }
};

// TextureParams fields in order:
// tapeMacro (TapeMacro), filterCutoffBias, filterResonanceBias,
// delayEnabled, delayBeats, delayFeedback, delayMix,
// bassBoostDB, trebleBoostDB

const TextureParams kTexturePresets[kTextureModeCount] = {
    // CLEAN - Transparent, bright
    { {3, 5, 8, 85, 0},  0, 0,  false, 0, 0, 0,  0, 0 },
    // DUB - Space, delay, warmth
    { {10, 15, 10, 68, 0},  -100, 0,  true, 0.75f, 0.5f, 0.50f,  2, -2 },
    // LOFI - Vintage, soft, dark
    { {15, 20, 12, 60, 0},  -150, -0.1f,  true, 0.5f, 0.3f, 0.15f,  3, -4 },
    // INDUSTRIAL - Harsh, bright, mechanical
    { {5, 30, 20, 75, 0},  100, 0.15f,  true, 0.25f, 0.2f, 0.1f,  1, 3 }
};

// F-key preset combinations
const GenrePreset kGenrePresets[8] = {
    { GenerativeMode::Acid, TextureMode::Clean, "Classic Acid" },       // F1
    { GenerativeMode::Outrun, TextureMode::Clean, "Outrun Lead" },      // F2
    { GenerativeMode::Darksynth, TextureMode::Clean, "Darksynth Bass" },// F3
    { GenerativeMode::Outrun, TextureMode::Dub, "Synthwave" },          // F4
    { GenerativeMode::Electro, TextureMode::Industrial, "EBM" },        // F5
    { GenerativeMode::Rave, TextureMode::Clean, "Rave Acid" },          // F6
    { GenerativeMode::Darksynth, TextureMode::Industrial, "Hotline" },  // F7
    { GenerativeMode::Electro, TextureMode::Clean, "Detroit" }          // F8
};

// ============================================================================
// TEXTURE APPLICATION
// ============================================================================

static float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

void GenreManager::applyGenreTimbre(MiniAcid& engine) {
    const GenreBehavior b = getBehavior();
    const GenreTimbre& t = b.timbre;

    for (int v = 0; v < 2; ++v) {
        // Apply base synthesis parameters (BEFORE texture bias)
        // Oscillator uses normalized index mapping
        engine.set303ParameterNormalized(TB303ParamId::Oscillator, t.osc, v);
        
        float cut = t.cutoff;
        float reso = t.resonance;
        float env = t.envAmount;
        float decay = t.envDecay;

        if (v == 0) {
            // Bass: keep it low, but not hard-constant
            cut = clamp01(cut);
            reso = clamp01(reso);
            env = clamp01(env);
            decay = clamp01(decay);

            // Force low-ish ranges (Soft Clamps)
            if (cut < 0.05f) cut = 0.05f;
            if (cut > 0.45f) cut = 0.45f;

            if (env < 0.02f) env = 0.02f;
            if (env > 0.20f) env = 0.20f;

            if (decay < 0.04f) decay = 0.04f;
            if (decay > 0.25f) decay = 0.25f;

            if (reso < 0.20f) reso = 0.20f;
            if (reso > 0.85f) reso = 0.85f;
        } else {
            // Lead: audibility floors, but NOT huge jumps
            if (cut < 0.40f) cut = 0.40f;
            if (env < 0.20f) env = 0.20f;
            if (decay < 0.08f) decay = 0.08f;

            // Cap to prevent “screech”
            if (cut > 0.95f) cut = 0.95f;
            if (reso > 0.95f) reso = 0.95f;
        }

        // Apply NORMALIZED parameters
        engine.set303ParameterNormalized(TB303ParamId::Cutoff,    clamp01(cut), v);
       // engine.set303ParameterNormalized(TB303ParamId::Resonance, clamp01(reso), v);
        engine.set303ParameterNormalized(TB303ParamId::EnvAmount, clamp01(env), v);
        engine.set303ParameterNormalized(TB303ParamId::EnvDecay,  clamp01(decay), v);
    }
}


void GenreManager::applyTexture(MiniAcid& engine) {
    const TextureParams& params = getTextureParams();
    
    // Apply Tape FX macro
    TapeState& tape = engine.sceneManager().currentScene().tape;
    tape.macro = params.tapeMacro;
    tape.fxEnabled = false;
    
    // Apply delay settings (using TempoDelay)
    // Apply delay settings (using TempoDelay) to both voices
    for (int i = 0; i < 2; i++) {
        auto& d = engine.tempoDelay(i);
        d.setEnabled(params.delayEnabled);
        if (params.delayEnabled) {
            d.setBeats(params.delayBeats);
            d.setFeedback(params.delayFeedback);
            d.setMix(params.delayMix);
        }
    }
    
    // Apply filter bias using DELTA to prevent drift on repeated calls
    // We store last applied bias and only apply the difference
    int newCutoffBias = computeCutoffBiasSteps();
    int newResBias = computeResBiasSteps();
    
    int cutoffDelta = newCutoffBias - lastAppliedCutoffBias_;
    int resDelta = newResBias - lastAppliedResBias_;
    
    if (cutoffDelta != 0) {
        engine.adjust303Parameter(TB303ParamId::Cutoff, cutoffDelta, 0);
        engine.adjust303Parameter(TB303ParamId::Cutoff, cutoffDelta, 1);
        lastAppliedCutoffBias_ = newCutoffBias;
    }
    
    if (resDelta != 0) {
        engine.adjust303Parameter(TB303ParamId::Resonance, resDelta, 0);
        engine.adjust303Parameter(TB303ParamId::Resonance, resDelta, 1);
        lastAppliedResBias_ = newResBias;
    }
}

// ============================================================================
// STRUCTURAL BEHAVIOR (stepMask + motif + scale)
// ============================================================================

GenreBehavior GenreManager::getBehavior() const {
    // Base behavior per generative mode (structural skeleton)
    // stepMask: which steps can have notes (bitmask 0-15)
    // motifLength: coherent phrase length
    // preferredScale: 0=MinorPent, 1=Phrygian, 2=Aeolian, 3=Dorian
    static const GenreBehavior kBase[kGenerativeModeCount] = {
        // Acid: all steps, 4-note motif, Phrygian, chromatic + octave jumps
        // Timbre: Saw(0.0), Bright filter, Med resonance, Strong env
        { 0xFFFF, 4, 1, true,  true,  true,  false,
          { 0.0f, 0.55f, 0.35f, 0.85f, 0.35f } },

        // Outrun (was Minimal): 16ths, 6-note motif, Minor scale, bright lead/arp
        // Timbre: Saw(0.0), High cutoff, Low res, Med env, Plucky
        { 0xFFFF, 6, 2, true,  false, true,  false,
          { 0.0f, 0.72f, 0.18f, 0.58f, 0.30f } },

        // Darksynth (was Hypnotic): 8ths gated, 3-note motif, Phrygian, evil bass
        // Timbre: Square(1.0), Low cutoff, High res, Strong env, Short gate
        { 0xAAAA, 3, 1, true,  false, false, false,
          { 1.0f, 0.34f, 0.50f, 0.92f, 0.22f } },

        // Electro: syncopated, 3-note motif, chromatic but tight octave
        // Timbre: Saw-ish(0.2), Sharp env, Short decay
        { 0xAA55, 3, 3, true,  true,  false, false,
          { 0.2f, 0.60f, 0.30f, 0.75f, 0.20f } },

        // Rave: dense, longer motif, chromatic + jumps
        // Timbre: Saw(0.0), Open filter, Aggressive
        { 0xFFFF, 6, 1, true,  true,  true,  false,
          { 0.0f, 0.78f, 0.32f, 0.80f, 0.50f } }
    };

    GenreBehavior b = kBase[static_cast<int>(state_.generative)];

    // Texture influences TIMBRE only, NOT rhythm/stepMask
    // (User feedback: changing stepMask makes genres unreadable)
    if (state_.texture == TextureMode::Industrial) {
        b.allowChromatic = true; // Allow dissonance for harsh texture
    }

    return b;
}
