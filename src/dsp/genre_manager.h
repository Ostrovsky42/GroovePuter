#ifndef GENRE_MANAGER_H
#define GENRE_MANAGER_H
#include <stdint.h>
#include <cstdio>
#include "src/dsp/mini_dsp_params.h"
#include "src/dsp/tape_defs.h"

// ============================================================================
// TWO-AXIS GENRE SYSTEM
// Generative Mode × Texture Mode = 20 genre combinations
// ============================================================================

// === AXIS 1: GENERATIVE MODE (how patterns are created) ===
enum class GenerativeMode : uint8_t {
    Acid = 0,       // Melodic, slides, 8-14 notes
    Outrun = 1,     // Minimal (Outrun): bright lead/arp, synthwave
    Darksynth = 2,  // Techno (Darksynth): evil bass, aggressive gated
    Electro = 3,    // Staccato, syncopated, mechanical
    Rave = 4,       // Dense, high energy, 12-16 notes
    Reggae = 5,     // Sparse, offbeat, dub-friendly
    TripHop = 6,    // Slow, gritty, humanized
    Broken = 7,     // Syncopated, broken-beat feel
    Chip = 8        // Retro console style, quantized and tight
};

// === AXIS 2: TEXTURE MODE (how sound is processed) ===
enum class TextureMode : uint8_t {
    Clean = 0,     // Transparent, bright
    Dub = 1,       // Space, delay, warmth
    LoFi = 2,      // Vintage, soft, dark
    Industrial = 3, // Harsh, bright, mechanical
    Psychedelic = 4 // Wide, animated, bright
};

static constexpr int kGenerativeModeCount = 9;
static constexpr int kTextureModeCount = 5;
using GenreRecipeId = uint8_t;
static constexpr GenreRecipeId kBaseRecipeId = 0;

// === GENERATIVE PARAMETERS ===
struct GenerativeParams {
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

// === GROOVE RECIPE (Data-Driven bridge for generators) ===
struct GrooveRecipe {
    uint8_t      stepsPerBar = 16;     // 8, 16, 32
    uint8_t      swingPercent = 50;    // 50–75
    float        gateLengthRatio = 0.5f;
    float        densityMin = 0.25f;   // percentage of steps filled
    float        densityMax = 0.75f;
    uint8_t      velMin = 60, velMax = 127;
    uint16_t     swingMask = 0;        // Bitmask for which voices swing (VoiceId bits)
    bool         sparseKick = false;
    bool         noAccents = false;
    bool         preferDownbeats = true;
};

// === TEXTURE PARAMETERS ===
struct TextureParams {
    // Tape FX
    TapeMacro tapeMacro;
    
    // Filter bias (added to current cutoff/res)
    float filterCutoffBias;   // -200 to +200 Hz
    float filterResonanceBias; // -0.2 to +0.2
    
    // Delay
    bool delayEnabled;
    float delayBeats;         // delay time in beats (BPM-synced)
    float delayFeedback;      // 0-1
    float delayMix;           // 0-1
    
    // Master EQ
    float bassBoostDB;        // -6 to +6
    float trebleBoostDB;      // -6 to +6
};

// === GENRE TIMBRE (base synthesis parameters, no FX) ===
struct GenreTimbre {
    float osc;        // 0..1 (0.0=Saw, 1.0=Square)
    float cutoff;     // 0..1
    float resonance;  // 0..1
    float envAmount;  // 0..1 
    float envDecay;   // 0..1
};

// === GENRE BEHAVIOR (structural, not probabilistic) ===
struct GenreBehavior {
    uint16_t stepMask;        // Allowed steps (bitmask, 16 bits = 16 steps)
    uint8_t  motifLength;     // Phrase length 1..8
    uint8_t  preferredScale;  // Index into kScales array
    bool     useMotif;        // Generate coherent phrase vs random notes
    bool     allowChromatic;  // Allow passing tones outside scale
    bool     forceOctaveJump; // Encourage octave jumps
    bool     avoidClusters;   // Forbid adjacent notes (for minimal/hypnotic)
    GenreTimbre timbre;       // Base synthesis parameters
};

// === PRESET TABLES (defined in genre_manager.cpp) ===
extern const GenerativeParams kGenerativePresets[kGenerativeModeCount];
extern const TextureParams kTexturePresets[kTextureModeCount];

// === GENRE STATE ===
struct GenreState {
    GenerativeMode generative = GenerativeMode::Acid;
    TextureMode texture = TextureMode::Clean;
    GenreRecipeId recipe = 0;      // 0 = base, no subgenre recipe override
    GenreRecipeId morphTarget = 0; // 0 = none
    uint8_t morphAmount = 0;       // 0..255
    char cachedName_[32] = "Acid";  // Cached, not recalculated in draw()
    
    // Call when mode changes
    void updateCachedName() {
        static const char* const genNames[] = {
            "Acid", "Minimal", "Techno", "Electro", "Rave",
            "Reggae", "TripHop", "Broken", "Chip"
        };
        static const char* const texNames[] = {"", "Dub ", "LoFi ", "Industrial ", "Psy "};
        snprintf(cachedName_, sizeof(cachedName_), "%s%s", 
                 texNames[static_cast<int>(texture)],
                 genNames[static_cast<int>(generative)]);
    }
    
    const char* getName() const { return cachedName_; }
};


// Forward declaration
class MiniAcid;
struct DrumGenreTemplate;

// === GENRE MANAGER ===
class GenreManager {
public:
    GenreManager() : state_() {
        state_.updateCachedName();
    }
    
    // Setters (update cache on change)
    void setGenerativeMode(GenerativeMode mode) { 
        state_.generative = mode; 
        state_.updateCachedName();
        cachedDirty_ = true;
    }
    void setTextureMode(TextureMode mode) { 
        state_.texture = mode; 
        state_.updateCachedName();
        cachedDirty_ = true;
    }
    void setRecipe(GenreRecipeId recipe) {
        state_.recipe = recipe;
        cachedDirty_ = true;
    }
    void setMorphTarget(GenreRecipeId target) {
        state_.morphTarget = target;
        cachedDirty_ = true;
    }
    void setMorphAmount(uint8_t amount) {
        state_.morphAmount = amount;
        cachedDirty_ = true;
    }
    
    // Cyclers
    void cycleGenerative(int direction = 1) {
        int next = (static_cast<int>(state_.generative) + direction + kGenerativeModeCount) % kGenerativeModeCount;
        state_.generative = static_cast<GenerativeMode>(next);
        state_.updateCachedName();
        cachedDirty_ = true;
    }
    
    void cycleTexture(int direction = 1) {
        int next = (static_cast<int>(state_.texture) + direction + kTextureModeCount) % kTextureModeCount;
        state_.texture = static_cast<TextureMode>(next);
        state_.updateCachedName();
        cachedDirty_ = true;
    }
    void queueRecipe(GenreRecipeId recipe) {
        pendingRecipe_ = recipe;
        pendingRecipeDirty_ = true;
    }
    void queueMorphTarget(GenreRecipeId target) {
        pendingMorphTarget_ = target;
        pendingMorphDirty_ = true;
    }
    void queueMorphAmount(uint8_t amount) {
        pendingMorphAmount_ = amount;
        pendingMorphDirty_ = true;
    }
    bool commitPendingRecipe() {
        bool changed = false;
        if (pendingRecipeDirty_) {
            state_.recipe = pendingRecipe_;
            pendingRecipeDirty_ = false;
            changed = true;
        }
        if (pendingMorphDirty_) {
            state_.morphTarget = pendingMorphTarget_;
            state_.morphAmount = pendingMorphAmount_;
            pendingMorphDirty_ = false;
            changed = true;
        }
        if (changed) cachedDirty_ = true;
        return changed;
    }
    
    // Getters
    GenerativeMode generativeMode() const { return state_.generative; }
    TextureMode textureMode() const { return state_.texture; }
    GenreRecipeId recipe() const { return state_.recipe; }
    GenreRecipeId morphTarget() const { return state_.morphTarget; }
    uint8_t morphAmount() const { return state_.morphAmount; }
    void cycleRecipe(int direction = 1);
    const char* getCurrentGenreName() const { return state_.getName(); }
    GenreState& state() { return state_; }
    const GenreState& state() const { return state_; }
    
    // Get parameters
    const GenerativeParams& getGenerativeParams() const { return getCompiledGenerativeParams(); }

    // Compiled params (base preset + recipe + morph)
    const GenerativeParams& getCompiledGenerativeParams() const;

    // Get the high-level recipe for a specific generative mode (or current if none)
    GrooveRecipe getGrooveRecipe() const;

    // Optional drum override from recipe (nullptr => fallback to kDrumTemplates)
    const DrumGenreTemplate* drumTemplateOverride() const;
    
    const TextureParams& getTextureParams() const {
        return kTexturePresets[static_cast<int>(state_.texture)];
    }
    
    // Get structural behavior (stepMask, motif, scale)
    GenreBehavior getBehavior() const;
    
    // Mode name helpers
    static const char* generativeModeName(GenerativeMode mode) {
        static const char* const names[] = {
            "Acid", "Minimal", "Techno", "Electro", "Rave",
            "Reggae", "TripHop", "Broken", "Chip"
        };
        return names[static_cast<int>(mode)];
    }
    
    static const char* textureModeName(TextureMode mode) {
        static const char* const names[] = {"Clean", "Dub", "LoFi", "Industrial", "Psychedelic"};
        return names[static_cast<int>(mode)];
    }
    static const char* recipeName(GenreRecipeId id);
    static uint8_t recipeCount();
    static GrooveboxMode grooveboxModeForRecipe(GenreRecipeId id, GenerativeMode fallbackMode);

    // Canonical bridge between 9 genres and 5 groovebox macro modes.
    static GrooveboxMode grooveboxModeForGenerative(GenerativeMode mode);

    // Curated compatibility helpers
    static bool isTextureAllowed(GenerativeMode genre, TextureMode texture);
    static TextureMode firstAllowedTexture(GenerativeMode genre);
    static TextureMode nextAllowedTexture(GenerativeMode genre, TextureMode current, int direction = 1);

    
    // Apply texture to engine (implemented in cpp)
    void applyTexture(MiniAcid& engine);
    
    // Apply genre timbre (base synthesis params) to engine
    void applyGenreTimbre(MiniAcid& engine);
    
    // Reset bias tracking (call on engine reset or scene load)
    void resetTextureBiasTracking() {
        lastAppliedCutoffBias_ = 0;
        lastAppliedResBias_ = 0;
    }
    
    // Sync baseline to current texture WITHOUT applying delta
    // Use after loading scene params to mark current texture as "already applied"
    void syncTextureBiasBaselineFromCurrentState() {
        lastAppliedCutoffBias_ = computeCutoffBiasSteps();
        lastAppliedResBias_ = computeResBiasSteps();
    }
    
private:
    GenreState state_;
    GenreRecipeId pendingRecipe_ = 0;
    bool pendingRecipeDirty_ = false;
    GenreRecipeId pendingMorphTarget_ = 0;
    uint8_t pendingMorphAmount_ = 0;
    bool pendingMorphDirty_ = false;
    mutable bool cachedDirty_ = true;
    mutable GenerativeParams cachedGenerativeParams_{};
    mutable const DrumGenreTemplate* cachedDrumOverride_ = nullptr;
    
    // Track last applied filter bias for delta calculation (idempotent)
    int lastAppliedCutoffBias_ = 0;
    int lastAppliedResBias_ = 0;
    
    // Compute bias steps from current texture params
    int computeCutoffBiasSteps() const {
        const TextureParams& p = getTextureParams();
        return (int)(p.filterCutoffBias / 5.0f);
    }
    
    int computeResBiasSteps() const {
        const TextureParams& p = getTextureParams();
        return (int)(p.filterResonanceBias * 40.0f);
    }

    void ensureCompiled_() const;
};

// === F-KEY PRESET COMBINATIONS ===
struct GenrePreset {
    GenerativeMode generative;
    TextureMode texture;
    const char* name;
};

extern const GenrePreset kGenrePresets[8];
static constexpr int kGenrePresetCount = 8;

#endif // GENRE_MANAGER_H
