#include "genre_manager.h"
#include "miniacid_engine.h"
#include "drum_genre_templates.h"
#include <cmath>

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
// sparseKick, sparseHats, noAccents, fillProbability,
// drumSyncopation, drumPreferOffbeat, drumVoiceCount

const GenerativeParams kGenerativePresets[kGenerativeModeCount] = {
    // ACID - Melodic, slides, aggressive
    { 8, 14, 36, 72,  0.40f, 0.50f, 0.8f,  0.0f, 0.1f,  85, 120,  false, true, 0.25f, 0.10f, 0.15f,  false, false, false, 0.6f,  0.20f, false, 8 },
    // MINIMAL (Outrun) - Bright lead/arp, 80s synthwave (swing LOW for drive)
    { 10, 14, 48, 72,  0.12f, 0.25f, 0.70f,  0.08f, 0.01f,  90, 118,  false, true, 0.10f, 0.03f, 0.05f,  false, false, false, 0.4f,  0.12f, false, 6 },
    // TECHNO (Darksynth) - Evil bass (swing ZERO, repeats ON for bass anchor)
    { 4, 7, 24, 48,  0.05f, 0.50f, 0.35f,  0.0f, 0.0f,  100, 125,  true, true, 0.70f, 0.0f, 0.03f,  false, true, false, 0.25f,  0.05f, false, 8 },
    // ELECTRO - Staccato, syncopated, mechanical
    { 6, 10, 36, 60,  0.0f, 0.70f, 0.3f,  0.0f, 0.0f,  105, 115,  false, true, 0.30f, 0.05f, 0.10f,  false, false, false, 0.5f,  0.35f, false, 8 },
    // RAVE - Dense, high energy
    { 12, 16, 36, 72,  0.20f, 0.80f, 0.5f,  0.0f, 0.0f,  110, 127,  false, true, 0.20f, 0.05f, 0.20f,  false, false, false, 0.7f,  0.08f, false, 8 },
    // REGGAE - Sparse, offbeat, warm bass
    { 4, 8, 24, 48,  0.05f, 0.15f, 0.55f,  0.20f, 0.15f,  80, 110,  false, true, 0.60f, 0.12f, 0.05f,  true, true, true, 0.25f,  0.28f, true, 6 },
    // TRIPHOP - Slow, gritty, humanized
    { 5, 9, 36, 60,  0.05f, 0.25f, 0.60f,  0.18f, 0.25f,  75, 108,  false, true, 0.35f, 0.18f, 0.10f,  true, true, false, 0.20f,  0.30f, true, 6 },
    // BROKEN - Syncopated, broken-beat feel
    { 7, 12, 36, 72,  0.10f, 0.35f, 0.45f,  0.28f, 0.12f,  90, 120,  false, true, 0.20f, 0.08f, 0.12f,  false, false, false, 0.35f,  0.45f, true, 8 },
    // CHIP - Retro console style, very tight and quantized
    { 8, 12, 48, 72,  0.02f, 0.15f, 0.38f,  0.0f, 0.0f,  96, 122,  true, true, 0.40f, 0.02f, 0.06f,  false, true, true, 0.12f,  0.02f, false, 4 }
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
    { {5, 30, 20, 75, 0},  100, 0.15f,  true, 0.25f, 0.2f, 0.1f,  1, 3 },
    // PSYCHEDELIC - Wider movement, brighter top, long tails
    { {18, 35, 22, 78, 1},  120, 0.10f,  true, 0.75f, 0.62f, 0.42f,  2, 4 }
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
static float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static int lerpi(int a, int b, float t) {
    return static_cast<int>(std::lround(static_cast<float>(a) + (static_cast<float>(b - a) * t)));
}

namespace {
// Bitmask per GenerativeMode: bit N means TextureMode N is allowed.
// 0=Clean 1=Dub 2=LoFi 3=Industrial 4=Psychedelic
constexpr uint8_t kAllowedTextureMask[kGenerativeModeCount] = {
    0b11111, // Acid: Clean, Dub, LoFi, Industrial, Psy
    0b00111, // Outrun/Minimal: Clean, Dub, LoFi
    0b11111, // Darksynth/Techno: Clean, Dub, LoFi, Industrial, Psy
    0b11011, // Electro: Clean, Dub, Industrial, Psy
    0b11001, // Rave: Clean, Industrial, Psy
    0b00111, // Reggae: Clean, Dub, LoFi
    0b00111, // TripHop: Clean, Dub, LoFi
    0b11111, // Broken: Clean, Dub, LoFi, Industrial, Psy
    0b10101  // Chip: Clean, LoFi, Psy
};

struct RecipeOverride {
    int minNotes = -1;
    int maxNotes = -1;
    float swingAmount = -1.0f;
    float microTimingAmount = -1.0f;
    int velocityMin = -1;
    int velocityMax = -1;
    float rootNoteBias = -1.0f;
    float ghostProbability = -1.0f;
    float chromaticProbability = -1.0f;
    float fillProbability = -1.0f;
    int sparseKick = -1;
    int sparseHats = -1;
    int noAccents = -1;
    float drumSyncopation = -1.0f;
    int drumPreferOffbeat = -1;
    int drumVoiceCount = -1;
};

struct GenreRecipeDef {
    GenreRecipeId id = 0;
    const char* name = "";
    RecipeOverride params;
    bool hasDrumOverride = false;
    DrumGenreTemplate drum{};
};

static void applyRecipeOverride(GenerativeParams& p, const RecipeOverride& o) {
    if (o.minNotes >= 0) p.minNotes = o.minNotes;
    if (o.maxNotes >= 0) p.maxNotes = o.maxNotes;
    if (o.swingAmount >= 0.0f) p.swingAmount = o.swingAmount;
    if (o.microTimingAmount >= 0.0f) p.microTimingAmount = o.microTimingAmount;
    if (o.velocityMin >= 0) p.velocityMin = o.velocityMin;
    if (o.velocityMax >= 0) p.velocityMax = o.velocityMax;
    if (o.rootNoteBias >= 0.0f) p.rootNoteBias = o.rootNoteBias;
    if (o.ghostProbability >= 0.0f) p.ghostProbability = o.ghostProbability;
    if (o.chromaticProbability >= 0.0f) p.chromaticProbability = o.chromaticProbability;
    if (o.fillProbability >= 0.0f) p.fillProbability = o.fillProbability;
    if (o.sparseKick >= 0) p.sparseKick = (o.sparseKick != 0);
    if (o.sparseHats >= 0) p.sparseHats = (o.sparseHats != 0);
    if (o.noAccents >= 0) p.noAccents = (o.noAccents != 0);
    if (o.drumSyncopation >= 0.0f) p.drumSyncopation = o.drumSyncopation;
    if (o.drumPreferOffbeat >= 0) p.drumPreferOffbeat = (o.drumPreferOffbeat != 0);
    if (o.drumVoiceCount >= 0) p.drumVoiceCount = o.drumVoiceCount;
}

static void clampGenerativeParams(GenerativeParams& p) {
    if (p.minNotes < 0) p.minNotes = 0;
    if (p.maxNotes < p.minNotes) p.maxNotes = p.minNotes;
    if (p.maxNotes > 16) p.maxNotes = 16;
    if (p.minNotes > 16) p.minNotes = 16;

    if (p.minOctave > p.maxOctave) {
        const int tmp = p.minOctave;
        p.minOctave = p.maxOctave;
        p.maxOctave = tmp;
    }

    p.slideProbability = clamp01(p.slideProbability);
    p.accentProbability = clamp01(p.accentProbability);
    p.gateLengthMultiplier = p.gateLengthMultiplier < 0.1f ? 0.1f : (p.gateLengthMultiplier > 1.0f ? 1.0f : p.gateLengthMultiplier);
    p.swingAmount = p.swingAmount < 0.0f ? 0.0f : (p.swingAmount > 0.66f ? 0.66f : p.swingAmount);
    p.microTimingAmount = clamp01(p.microTimingAmount);
    p.velocityMin = p.velocityMin < 1 ? 1 : (p.velocityMin > 127 ? 127 : p.velocityMin);
    p.velocityMax = p.velocityMax < p.velocityMin ? p.velocityMin : (p.velocityMax > 127 ? 127 : p.velocityMax);
    p.rootNoteBias = clamp01(p.rootNoteBias);
    p.ghostProbability = clamp01(p.ghostProbability);
    p.chromaticProbability = clamp01(p.chromaticProbability);
    p.fillProbability = clamp01(p.fillProbability);
    p.drumSyncopation = clamp01(p.drumSyncopation);
    p.drumVoiceCount = p.drumVoiceCount < 1 ? 1 : (p.drumVoiceCount > 8 ? 8 : p.drumVoiceCount);
}

// 0 = Base (no override). New recipes live here without touching GenerativeMode count.
static const GenreRecipeDef kGenreRecipes[] = {
    {1, "UK Garage",
     {6, 10, 0.28f, 0.18f, 84, 116, 0.45f, 0.12f, 0.10f, 0.28f, 0, 0, 0, 0.55f, 1, 8},
     true,
     {0x8121, 0x0808, 0xFFFF, 0x2222, 0.08f, 0.10f, 0.35f, 104, 102, 82, false, true}},
    {2, "Drum&Bass",
     {7, 12, 0.08f, 0.10f, 96, 124, 0.40f, 0.06f, 0.10f, 0.35f, 0, 0, 0, 0.65f, 1, 8},
     true,
     {0x8060, 0x0808, 0xFFFF, 0x2222, 0.05f, 0.08f, 0.25f, 118, 110, 98, false, false}},
    {3, "Footwork",
     {8, 12, 0.00f, 0.22f, 90, 120, 0.35f, 0.08f, 0.14f, 0.30f, 0, 0, 0, 0.80f, 1, 8},
     true,
     {0x9129, 0x0808, 0xFFFF, 0x1111, 0.10f, 0.10f, 0.42f, 112, 108, 90, false, true}},
    {4, "Psytrance",
     {12, 16, 0.00f, 0.04f, 102, 126, 0.22f, 0.05f, 0.10f, 0.55f, 0, 0, 0, 0.12f, 0, 8},
     true,
     {0x8888, 0x0808, 0xFFFF, 0x2222, 0.04f, 0.04f, 0.15f, 122, 112, 102, false, true}},
    {5, "Dub Techno",
     {3, 6, 0.12f, 0.16f, 72, 108, 0.78f, 0.28f, 0.05f, 0.18f, 1, 1, 1, 0.30f, 1, 6},
     true,
     {0x8080, 0x0808, 0x2222, 0x0202, 0.10f, 0.06f, 0.22f, 92, 88, 70, true, false}},
};

static const GenreRecipeDef* findRecipe(GenreRecipeId id) {
    if (id == 0) return nullptr;
    for (const auto& r : kGenreRecipes) {
        if (r.id == id) return &r;
    }
    return nullptr;
}

} // namespace

uint8_t GenreManager::recipeCount() {
    return static_cast<uint8_t>(1 + (sizeof(kGenreRecipes) / sizeof(kGenreRecipes[0])));
}

const char* GenreManager::recipeName(GenreRecipeId id) {
    if (id == kBaseRecipeId) return "BASE";
    const GenreRecipeDef* r = findRecipe(id);
    return r ? r->name : "BASE";
}

void GenreManager::cycleRecipe(int direction) {
    const uint8_t count = recipeCount();
    if (count == 0) return;
    int next = static_cast<int>(state_.recipe) + direction;
    while (next < 0) next += count;
    while (next >= count) next -= count;
    state_.recipe = static_cast<GenreRecipeId>(next);
    cachedDirty_ = true;
}

bool GenreManager::isTextureAllowed(GenerativeMode genre, TextureMode texture) {
    const int g = static_cast<int>(genre);
    const int t = static_cast<int>(texture);
    if (g < 0 || g >= kGenerativeModeCount) return false;
    if (t < 0 || t >= kTextureModeCount) return false;
    return (kAllowedTextureMask[g] & (1u << t)) != 0;
}

GrooveboxMode GenreManager::grooveboxModeForGenerative(GenerativeMode mode) {
    switch (mode) {
        case GenerativeMode::Acid:      return GrooveboxMode::Acid;
        case GenerativeMode::Outrun:    return GrooveboxMode::Minimal;
        case GenerativeMode::Darksynth: return GrooveboxMode::Electro;
        case GenerativeMode::Electro:   return GrooveboxMode::Electro;
        case GenerativeMode::Rave:      return GrooveboxMode::Acid;
        case GenerativeMode::Reggae:    return GrooveboxMode::Dub;
        case GenerativeMode::TripHop:   return GrooveboxMode::Dub;
        case GenerativeMode::Broken:    return GrooveboxMode::Breaks;
        case GenerativeMode::Chip:      return GrooveboxMode::Electro;
        default:                        return GrooveboxMode::Minimal;
    }
}

GrooveboxMode GenreManager::grooveboxModeForRecipe(GenreRecipeId id, GenerativeMode fallbackMode) {
    switch (id) {
        case 1: return GrooveboxMode::Breaks; // UK Garage
        case 2: return GrooveboxMode::Breaks; // DnB
        case 3: return GrooveboxMode::Breaks; // Footwork
        case 4: return GrooveboxMode::Acid;   // Psytrance
        case 5: return GrooveboxMode::Dub;    // Dub Techno
        case 0: break;                        // base layer: use fallback
        default: break;                       // unknown recipe: safe fallback
    }
    return grooveboxModeForGenerative(fallbackMode);
}

TextureMode GenreManager::firstAllowedTexture(GenerativeMode genre) {
    for (int t = 0; t < kTextureModeCount; ++t) {
        TextureMode mode = static_cast<TextureMode>(t);
        if (isTextureAllowed(genre, mode)) return mode;
    }
    return TextureMode::Clean;
}

TextureMode GenreManager::nextAllowedTexture(GenerativeMode genre, TextureMode current, int direction) {
    if (direction == 0) return current;
    int start = static_cast<int>(current);
    for (int i = 0; i < kTextureModeCount; ++i) {
        start = (start + direction + kTextureModeCount) % kTextureModeCount;
        TextureMode mode = static_cast<TextureMode>(start);
        if (isTextureAllowed(genre, mode)) return mode;
    }
    return firstAllowedTexture(genre);
}

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

            // Do not force a resonance floor for bass; allow fully clean low-end.
            if (reso < 0.0f) reso = 0.0f;
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
        engine.set303ParameterNormalized(TB303ParamId::Resonance, clamp01(reso), v);
        engine.set303ParameterNormalized(TB303ParamId::EnvAmount, clamp01(env), v);
        engine.set303ParameterNormalized(TB303ParamId::EnvDecay,  clamp01(decay), v);
    }
}


void GenreManager::applyTexture(MiniAcid& engine) {
    const TextureParams& params = getTextureParams();
    const float amount = clamp01(engine.sceneManager().currentScene().genre.textureAmount / 100.0f);
    
    // Apply Tape FX macro
    TapeState& tape = engine.sceneManager().currentScene().tape;
    TapeMacro macro = params.tapeMacro;
    macro.wow = static_cast<uint8_t>(macro.wow * amount);
    macro.age = static_cast<uint8_t>(macro.age * amount);
    macro.sat = static_cast<uint8_t>(macro.sat * amount);
    macro.crush = static_cast<uint8_t>(macro.crush * amount);
    const int neutralTone = 85;
    macro.tone = static_cast<uint8_t>(neutralTone + static_cast<int>((static_cast<int>(params.tapeMacro.tone) - neutralTone) * amount));
    tape.macro = macro;
    // FEEL page owns Tape ON/OFF. Genre texture adjusts macro only.
    
    // Apply delay settings (using TempoDelay)
    // Apply delay settings (using TempoDelay) to both voices
    for (int i = 0; i < 2; i++) {
        auto& d = engine.tempoDelay(i);
        const bool delayOn = params.delayEnabled && amount > 0.01f;
        d.setEnabled(delayOn);
        if (delayOn) {
            d.setBeats(params.delayBeats);
            d.setFeedback(params.delayFeedback * amount);
            d.setMix(params.delayMix * amount);
        }
    }
    
    // Apply filter bias using DELTA to prevent drift on repeated calls
    // We store last applied bias and only apply the difference
    int newCutoffBias = static_cast<int>((params.filterCutoffBias * amount) / 5.0f);
    int newResBias = static_cast<int>((params.filterResonanceBias * amount) * 40.0f);
    
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

        // Minimal (Outrun): 16ths, 6-note motif, Minor scale, bright lead/arp
        // Timbre: Saw(0.0), High cutoff, Low res, Med env, Plucky
        { 0xFFFF, 6, 2, true,  false, true,  false,
          { 0.0f, 0.72f, 0.18f, 0.58f, 0.30f } },

        // Techno (Darksynth): 8ths gated, 3-note motif, Phrygian, evil bass
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
          { 0.0f, 0.78f, 0.32f, 0.80f, 0.50f } },

        // Reggae: offbeat focus, short motif, minor pentatonic
        // Timbre: Square-ish, low cutoff, warm
        { 0xAAAA, 4, 0, true,  false, false, true,
          { 1.0f, 0.28f, 0.40f, 0.55f, 0.18f } },

        // TripHop: sparse, slow, humanized
        // Timbre: soft saw, mid cutoff, relaxed env
        { 0xF0F0, 4, 2, true,  false, false, true,
          { 0.2f, 0.45f, 0.25f, 0.55f, 0.30f } },

        // Broken: syncopated, lopsided groove
        // Timbre: bright-ish, fast env
        { 0xAA55, 3, 3, true,  true,  true,  false,
          { 0.0f, 0.62f, 0.32f, 0.70f, 0.25f } },

        // Chip: tight, stepped, game-console pulse flavor
        // Timbre: square, high env, short decay
        { 0xFFFF, 2, 0, true,  false, false, true,
          { 1.0f, 0.68f, 0.22f, 0.82f, 0.12f } }
    };

    GenreBehavior b = kBase[static_cast<int>(state_.generative)];

    return b;
}

void GenreManager::ensureCompiled_() const {
    if (!cachedDirty_) return;

    cachedGenerativeParams_ = kGenerativePresets[static_cast<int>(state_.generative)];
    cachedDrumOverride_ = nullptr;

    const GenreRecipeDef* baseRecipe = findRecipe(state_.recipe);
    if (baseRecipe) {
        applyRecipeOverride(cachedGenerativeParams_, baseRecipe->params);
        if (baseRecipe->hasDrumOverride) cachedDrumOverride_ = &baseRecipe->drum;
    }

    const GenreRecipeDef* morphRecipe = findRecipe(state_.morphTarget);
    if (morphRecipe && state_.morphAmount > 0 && state_.morphTarget != state_.recipe) {
        GenerativeParams target = kGenerativePresets[static_cast<int>(state_.generative)];
        applyRecipeOverride(target, morphRecipe->params);
        clampGenerativeParams(target);

        const float t = static_cast<float>(state_.morphAmount) / 255.0f;
        cachedGenerativeParams_.minNotes = lerpi(cachedGenerativeParams_.minNotes, target.minNotes, t);
        cachedGenerativeParams_.maxNotes = lerpi(cachedGenerativeParams_.maxNotes, target.maxNotes, t);
        cachedGenerativeParams_.swingAmount = lerpf(cachedGenerativeParams_.swingAmount, target.swingAmount, t);
        cachedGenerativeParams_.microTimingAmount = lerpf(cachedGenerativeParams_.microTimingAmount, target.microTimingAmount, t);
        cachedGenerativeParams_.velocityMin = lerpi(cachedGenerativeParams_.velocityMin, target.velocityMin, t);
        cachedGenerativeParams_.velocityMax = lerpi(cachedGenerativeParams_.velocityMax, target.velocityMax, t);
        cachedGenerativeParams_.rootNoteBias = lerpf(cachedGenerativeParams_.rootNoteBias, target.rootNoteBias, t);
        cachedGenerativeParams_.ghostProbability = lerpf(cachedGenerativeParams_.ghostProbability, target.ghostProbability, t);
        cachedGenerativeParams_.chromaticProbability = lerpf(cachedGenerativeParams_.chromaticProbability, target.chromaticProbability, t);
        cachedGenerativeParams_.fillProbability = lerpf(cachedGenerativeParams_.fillProbability, target.fillProbability, t);
        cachedGenerativeParams_.drumSyncopation = lerpf(cachedGenerativeParams_.drumSyncopation, target.drumSyncopation, t);
        cachedGenerativeParams_.drumVoiceCount = lerpi(cachedGenerativeParams_.drumVoiceCount, target.drumVoiceCount, t);
        if (t >= 0.5f) {
            cachedGenerativeParams_.sparseKick = target.sparseKick;
            cachedGenerativeParams_.sparseHats = target.sparseHats;
            cachedGenerativeParams_.noAccents = target.noAccents;
            cachedGenerativeParams_.drumPreferOffbeat = target.drumPreferOffbeat;
            if (morphRecipe->hasDrumOverride) cachedDrumOverride_ = &morphRecipe->drum;
        }
    }

    clampGenerativeParams(cachedGenerativeParams_);
    cachedDirty_ = false;
}

const GenerativeParams& GenreManager::getCompiledGenerativeParams() const {
    ensureCompiled_();
    return cachedGenerativeParams_;
}

const DrumGenreTemplate* GenreManager::drumTemplateOverride() const {
    ensureCompiled_();
    return cachedDrumOverride_;
}
