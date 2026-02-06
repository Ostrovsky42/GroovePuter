#include "mode_manager.h"
#include "grooveputer_engine.h"
#include <algorithm>

void GrooveboxModeManager::setMode(GrooveboxMode mode) {
    currentMode_ = mode;
    engine_.setGrooveboxMode(mode); // Sync to engine
}

void GrooveboxModeManager::toggle() {
    setMode((currentMode_ == GrooveboxMode::Acid) ? GrooveboxMode::Minimal : GrooveboxMode::Acid);
}

void GrooveboxModeManager::apply303Preset(int voiceIndex, int presetIndex) {
    int count;
    const TB303ModePreset* presets = get303Presets(count);
    if (presetIndex < 0 || presetIndex >= count) return;
    
    const TB303ModePreset& p = presets[presetIndex];
    engine_.set303Parameter(TB303ParamId::Cutoff, p.cutoff, voiceIndex);
    engine_.set303Parameter(TB303ParamId::Resonance, p.resonance, voiceIndex);
    engine_.set303Parameter(TB303ParamId::EnvAmount, p.envAmount, voiceIndex);
    // envDecay needs mapping from 0..1 to 20..2200
    float decayMs = 20.0f + p.decay * (2200.0f - 20.0f);
    engine_.set303Parameter(TB303ParamId::EnvDecay, decayMs, voiceIndex);
    
    if (voiceIndex == 0) {
        engine_.set303DistortionEnabled(0, p.distortion);
        engine_.set303DelayEnabled(0, p.delay);
    } else {
        engine_.set303DistortionEnabled(1, p.distortion);
        engine_.set303DelayEnabled(1, p.delay);
    }
}

struct Scale {
    const char* name;
    int intervals[7];
    int count;
};

const Scale kScales[] = {
    {"Minor Pentatonic", {0, 3, 5, 7, 10}, 5},
    {"Phrygian", {0, 1, 3, 5, 7, 8, 10}, 7},
    {"Aeolian", {0, 2, 3, 5, 7, 8, 10}, 7},
    {"Dorian", {0, 2, 3, 5, 7, 9, 10}, 7}
};

int quantizeToScale(int note, const Scale& scale) {
    // Protect against negative notes
    if (note < 0) return note;
    
    int octave = note / 12;
    // Normalize pitch for negative modulo behavior in C++
    int pitch = ((note % 12) + 12) % 12;
    
    int bestPitch = scale.intervals[0];
    int minDiff = 12;
    
    for (int i = 0; i < scale.count; i++) {
        int diff = abs(pitch - scale.intervals[i]);
        if (diff < minDiff) {
            minDiff = diff;
            bestPitch = scale.intervals[i];
        }
    }
    return octave * 12 + bestPitch;
}


void GrooveboxModeManager::generatePattern(SynthPattern& pattern, float bpm) const {
    const ModeConfig& cfg = config();  // Use reference, not copy!
    
    // BPM-adaptive parameters (computed locally to avoid stack overflow)
    // Normalize BPM to [0,1]: 0=slow (80), 1=fast (170)
    float t = (bpm - 80.0f) / (170.0f - 80.0f);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    
    // Linear interpolation helper
    auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
    
    // Compute BPM-adapted parameters based on mode
    int adaptedMinNotes, adaptedMaxNotes;
    float adaptedChromaticProb, adaptedGhostProb, adaptedRootBias, adaptedSwing;
    
    if (currentMode_ == GrooveboxMode::Acid) {
        // ACID: Fast = tight & clean, Slow = busy & chromatic
        adaptedMinNotes = (int)lerp(13.0f, 6.0f, t);
        adaptedMaxNotes = (int)lerp(16.0f, 8.0f, t);
        adaptedChromaticProb = lerp(0.18f, 0.06f, t);
        adaptedGhostProb = lerp(0.12f, 0.04f, t);
        adaptedRootBias = cfg.pattern.rootNoteBias;  // Use base config
        adaptedSwing = 0.0f;  // Always grid-tight
    } else {
        // MINIMAL: Fast = hypnotic & clean, Slow = deep & textured
        adaptedMinNotes = (int)lerp(5.0f, 2.0f, t);
        adaptedMaxNotes = (int)lerp(7.0f, 4.0f, t);
        adaptedChromaticProb = cfg.pattern.chromaticProbability;  // Use base
        adaptedGhostProb = lerp(0.35f, 0.12f, t);
        adaptedRootBias = lerp(0.70f, 0.85f, t);
        
        // Swing in milliseconds (18ms @ slow, 8ms @ fast)
        float swingMs = lerp(18.0f, 8.0f, t);
        adaptedSwing = swingMs / 100.0f;  // Normalize
    }
    
    // Standardized probability helper (float 0-1 -> int 0-100)
    auto prob100 = [](float p){ return int(p * 100.0f + 0.5f); };

    // Clear pattern
    for (int i = 0; i < SynthPattern::kSteps; ++i) {
        pattern.steps[i].note = -1; // -1 means no note
        pattern.steps[i].accent = false;
        pattern.steps[i].slide = false;
        pattern.steps[i].velocity = 100;
        pattern.steps[i].timing = 0;
        pattern.steps[i].ghost = false;
    }
    
    // Pick a scale for this pattern
    const Scale& scale = (currentMode_ == GrooveboxMode::Acid) ? kScales[0] : kScales[rand() % 4];
    int rootNote = (currentMode_ == GrooveboxMode::Acid) ? 36 : 24; // C2 or C1
    int strategy = rand() % 3; // 0: Random Walk, 1: Arp/Rhythm, 2: Sequential

    if (currentMode_ == GrooveboxMode::Acid) {
        // ACID STRATEGY: Melodic, chromatic, contrasting
        int currentScaleIdx = rand() % scale.count;
        int lastOctaveShift = 0;  // Track last octave shift to prevent ping-pong
        
        // Probability cache (using adapted values)
        int minNotesProb = adaptedMinNotes * 100 / 16 + 10;
        int chromaticProb = prob100(adaptedChromaticProb);
        int slideProb = prob100(cfg.pattern.slideProbability);
        int accentProb = prob100(cfg.pattern.accentProbability);
        int ghostProbVal = prob100(adaptedGhostProb);

        for (int i = 0; i < 16; i++) {
            bool shouldHit = (rand() % 100) < minNotesProb;
            
            if (shouldHit) {
                // Determine next note via strategy
                if (strategy == 0) { // Random walk in scale
                    currentScaleIdx = (currentScaleIdx + (rand() % 3) - 1 + scale.count) % scale.count;
                } else if (strategy == 1) { // Arp-like (root/3rd/5th focus)
                    int choices[] = {0, 0, 2, 4};
                    currentScaleIdx = choices[rand() % 4];
                } else { // High energy random
                    currentScaleIdx = rand() % scale.count;
                }

                int note = rootNote + scale.intervals[currentScaleIdx];
                
                // Chromatic passing tones (Acid character)
                if ((rand() % 100) < chromaticProb) {
                    note += (rand() % 3) - 1;  // +1, 0, -1 semitone
                }
                
                // Octave jumps with ping-pong prevention
                int octaveShift = 0;
                if ((rand() % 100) < 25) octaveShift = 12;
                else if ((rand() % 100) < 8) octaveShift = 24;
                else if ((rand() % 100) < 5) octaveShift = -12;
                
                // Prevent consecutive opposite jumps (ping-pong)
                if (lastOctaveShift != 0 && octaveShift != 0 && 
                    ((lastOctaveShift > 0 && octaveShift < 0) || (lastOctaveShift < 0 && octaveShift > 0))) {
                    octaveShift = 0;  // Cancel jump to prevent ping-pong
                }
                
                note += octaveShift;
                lastOctaveShift = octaveShift;

                pattern.steps[i].note = note;
                pattern.steps[i].velocity = cfg.pattern.velocityMin + 
                    (rand() % (cfg.pattern.velocityMax - cfg.pattern.velocityMin + 1));
                
                if ((rand() % 100) < slideProb) {
                    pattern.steps[i].slide = true;
                }
                if ((rand() % 100) < accentProb) {
                    pattern.steps[i].accent = true;
                    pattern.steps[i].velocity = 127;
                    if (rand() % 100 < 40) pattern.steps[i].slide = true;
                }
            } else if ((rand() % 100) < ghostProbVal) {
                // Ghost notes (punchy but quiet)
                pattern.steps[i].note = rootNote;
                pattern.steps[i].ghost = true;
                pattern.steps[i].accent = false;  // Ghosts never accented
                pattern.steps[i].slide = false;   // Ghosts never slide in Acid
                pattern.steps[i].velocity = cfg.pattern.ghostVelocityMin + 
                    (rand() % (cfg.pattern.ghostVelocityMax - cfg.pattern.ghostVelocityMin + 1));
            }
        }
    } else {
        // MINIMAL STRATEGY: Hypnotic, bass-heavy, textural
        // FIXED baseNote for stable hypnosis
        int baseNote = rootNote; 
        
        // Probability cache (using adapted values)
        int rootBiasProb = prob100(adaptedRootBias);
        int accentProb = prob100(cfg.pattern.accentProbability);
        int slideProb = prob100(cfg.pattern.slideProbability);
        int ghostProbVal = prob100(adaptedGhostProb);

        for (int i = 0; i < 16; i++) {
            // Rhythmic anchors: strong beats for hypnotic pulse
            bool isAnchor = (i % 4 == 0) || (i == 3) || (i == 6) || (i == 10);
            float hitProb = isAnchor ? 70.0f : 12.0f;  // Very sparse off-beats
            
            if ((rand() % 100) < hitProb) {
                // Focus on root (hypnotic repetition)
                int scaleIdx;
                
                if (rand() % 100 < rootBiasProb) {
                    scaleIdx = 0; // Root note
                } else if (rand() % 100 < 35) {
                    scaleIdx = (scale.count > 4) ? 4 : 2; // Fifth-ish
                } else {
                    scaleIdx = rand() % scale.count;
                }
                
                // "Rare escape" rule: force non-root on bar start sometimes
                if ((i == 0 || i == 8) && rand() % 100 < 20) {
                    scaleIdx = 1 + (rand() % (scale.count - 1));  // Any note but root
                }
                
                int note = baseNote + scale.intervals[scaleIdx];
                
                // Deep octave drops (bass emphasis)
                if (rand() % 100 < 15) note -= 12;
                
                pattern.steps[i].note = note;
                pattern.steps[i].velocity = cfg.pattern.velocityMin + 
                    (rand() % (cfg.pattern.velocityMax - cfg.pattern.velocityMin + 1));
                
                // Minimal accents are rare but precise
                if (i % 8 == 0 && rand() % 100 < accentProb) {
                    pattern.steps[i].accent = true;
                    pattern.steps[i].velocity = 127;
                }
                
                if (rand() % 100 < slideProb) {
                    pattern.steps[i].slide = true;
                }
            } else if ((rand() % 100) < ghostProbVal) {
                // Ghost notes: textural, quiet, slightly late
                pattern.steps[i].note = baseNote;
                pattern.steps[i].ghost = true;
                pattern.steps[i].accent = false;  // Ghosts never accented
                pattern.steps[i].slide = false;   // Ghosts never slide in Minimal
                pattern.steps[i].velocity = cfg.pattern.ghostVelocityMin + 
                    (rand() % (cfg.pattern.ghostVelocityMax - cfg.pattern.ghostVelocityMin + 1));
                // Micro-late timing for shuffle feel (+5 to +12 ticks) -- Minimal Only
                pattern.steps[i].timing = (int8_t)(5 + (rand() % 8));
            }
        }
    }

    // Apply Swing (only to off-beat notes)
    // Use adapted swing value
    if (adaptedSwing > 0.01f) {
        // Use 36 ticks for consistent swing timing across synth and drums
        int8_t swingTicks = (int8_t)(adaptedSwing * 36.0f);
        for (int i = 0; i < 16; i++) {
            if (i % 2 == 1 && pattern.steps[i].note >= 0) {
                pattern.steps[i].timing += swingTicks;
            }
        }
    }
}


void GrooveboxModeManager::generateDrumPattern(DrumPatternSet& patternSet) const {
    const ModeConfig& cfg = config();
    
    // Clear
    for (int v = 0; v < DrumPatternSet::kVoices; ++v) {
        for (int i = 0; i < DrumPattern::kSteps; ++i) {
            patternSet.voices[v].steps[i].hit = false;
            patternSet.voices[v].steps[i].accent = false;
            patternSet.voices[v].steps[i].velocity = 100;
            patternSet.voices[v].steps[i].timing = 0;
        }
    }
    
    // Use swing from synth pattern config (drums follow the same groove)
    float swingAmount = cfg.pattern.swingAmount;
    bool humanize = true;

    // Standardized probability helper
    auto prob100 = [](float p){ return int(p * 100.0f + 0.5f); };

    // KICK
    if (cfg.drums.sparseKick) {
        patternSet.voices[0].steps[0].hit = true;
        patternSet.voices[0].steps[0].velocity = 120;
        patternSet.voices[0].steps[8].hit = true;
        patternSet.voices[0].steps[8].velocity = 110;
        if (rand() % 100 < 30) {
            patternSet.voices[0].steps[10].hit = true;
            patternSet.voices[0].steps[10].velocity = 100;
        }
    } else {
        for (int i = 0; i < 16; i += 4) {
            patternSet.voices[0].steps[i].hit = true;
            patternSet.voices[0].steps[i].velocity = 115 - (i / 4) * 3;
        }
        // Occasional double kick
        if (rand() % 100 < 40) {
            patternSet.voices[0].steps[14].hit = true;
            patternSet.voices[0].steps[14].velocity = 90;
        }
    }
    
    // SNARE
    patternSet.voices[1].steps[4].hit = true;
    patternSet.voices[1].steps[4].velocity = 115;
    patternSet.voices[1].steps[12].hit = true;
    patternSet.voices[1].steps[12].velocity = 110;

    // Ghost Snare (shuffle feel)
    if (rand() % 100 < 50) {
        int pos = (rand() % 100 < 50) ? 3 : 11;
        patternSet.voices[1].steps[pos].hit = true;
        patternSet.voices[1].steps[pos].velocity = 30 + (rand() % 20);
        patternSet.voices[1].steps[pos].timing = (int8_t)((rand() % 10) - 5);
    }
    
    // HATS
    if (cfg.drums.sparseHats) {
        for (int i = 0; i < 16; i += 4) {
            patternSet.voices[2].steps[(i + 2) % 16].hit = true; // Off-beats
            patternSet.voices[2].steps[(i + 2) % 16].velocity = 100 + (rand() % 20);
        }
    } else {
        for (int i = 0; i < 16; i++) {
            bool accentBeat = (i % 2 == 0);
            if (accentBeat || (rand() % 100 < 40)) {
                patternSet.voices[2].steps[i].hit = true;
                patternSet.voices[2].steps[i].velocity = accentBeat ? (100 + (rand() % 20)) : (40 + (rand() % 30));
            }
        }
    }
    
    // OPEN HAT (Classic off-beat)
    for (int i = 0; i < 16; i += 4) {
        if (rand() % 100 < (currentMode_ == GrooveboxMode::Acid ? 60 : 30)) {
            int pos = i + 2;
            patternSet.voices[3].steps[pos].hit = true;
            patternSet.voices[3].steps[pos].velocity = 95;
            // OH chokes CH (conventionally handled by engine, but good for pattern logic)
            patternSet.voices[2].steps[pos].hit = false;
        }
    }
    
    // FILLS / PERC
    if ((rand() % 100) < prob100(cfg.drums.fillProbability)) {
        int count = 1 + (rand() % 3);
        int voice = 4 + (rand() % 4); // MT, HT, Rim, Clap
        for (int i = 0; i < count; i++) {
            int pos = 12 + (rand() % 4);
            patternSet.voices[voice].steps[pos & 15].hit = true;
            patternSet.voices[voice].steps[pos & 15].velocity = 80 + (i * 10);
            patternSet.voices[voice].steps[pos & 15].accent = (rand() % 100 < 30);
        }
    }
    
    // Humanize & Swing
    for (int i = 0; i < 16; i++) {
        int8_t swingTicks = (i % 2 == 1) ? (int8_t)(swingAmount * 36.0f) : 0;
        for (int v = 0; v < DrumPatternSet::kVoices; v++) {
            if (patternSet.voices[v].steps[i].hit) {
                if (humanize) {
                    int var = (rand() % 16) - 8;
                    int newVel = (int)patternSet.voices[v].steps[i].velocity + var;
                    patternSet.voices[v].steps[i].velocity = (uint8_t)std::max(1, std::min(127, newVel));
                    
                    // Micro-timing humanization
                    patternSet.voices[v].steps[i].timing += (int8_t)((rand() % 5) - 2);
                }
                patternSet.voices[v].steps[i].timing += swingTicks;
            }
        }
    }

    // ACCENTS
    if (!cfg.drums.noAccents) {
        for (int i = 0; i < 16; i += 4) { // Focus accents on beats
            if (rand() % 100 < 40) {
               int v = rand() % 2; // Mostly Kick or Snare
               if (patternSet.voices[v].steps[i].hit) {
                   patternSet.voices[v].steps[i].accent = true;
                   patternSet.voices[v].steps[i].velocity = 127;
               }
            }
        }
    }
}

// ============================================================================
// GENRE-BASED GENERATION (with structural behavior)
// ============================================================================

static inline bool stepAllowed(uint16_t mask, int step) {
    return (mask & (1u << step)) != 0;
}

static inline int prob100(float p) {
    int v = (int)(p * 100.0f + 0.5f);
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    return v;
}

// Fallback: params-only delegates to behavior version
void GrooveboxModeManager::generatePattern(SynthPattern& pattern, float bpm, const GenerativeParams& params) const {
    GenreBehavior defaultBehavior = { 0xFFFF, 4, 0, true, true, false, false };
    generatePattern(pattern, bpm, params, defaultBehavior);
}

// Full implementation with structural behavior + voice role
void GrooveboxModeManager::generatePattern(SynthPattern& pattern, float bpm, 
                                           const GenerativeParams& params, 
                                           const GenreBehavior& behavior,
                                           int voiceIndex) const {
    // Voice roles: 0 = bass (low, repetitive), 1 = lead/arp (high, melodic)
    const bool isBass = (voiceIndex == 0);
    
    // 1. Clear pattern
    for (int i = 0; i < SynthPattern::kSteps; ++i) {
        pattern.steps[i].note = -1;
        pattern.steps[i].accent = false;
        pattern.steps[i].slide = false;
        pattern.steps[i].ghost = false;
        pattern.steps[i].velocity = 100;
        pattern.steps[i].timing = 0;
    }

    // 2. Setup scale and root (VOICE-DEPENDENT)
    const Scale& scale = kScales[behavior.preferredScale % 4];
    int baseRoot;
    int octaveRange;
    
    if (isBass) {
        // Bass: low register, narrow range
        baseRoot = 24; // C1
        if (params.minOctave > 0 && params.minOctave < 36) baseRoot = params.minOctave;
        octaveRange = 1; // Stay within 1 octave
    } else {
        // Lead/Arp: higher register, wider range
        baseRoot = 48; // C3
        if (params.maxOctave > 48) baseRoot = 48 + (params.maxOctave - 48) / 3;
        octaveRange = 2; // Span 2 octaves
    }

    // 3. Generate MOTIF (VOICE-DEPENDENT)
    int motif[8];
    for (int i = 0; i < 8; i++) motif[i] = baseRoot;

    int motifLen = (int)behavior.motifLength;
    if (isBass) {
        // Bass: shorter motif, more repetition
        motifLen = std::min(motifLen, 3);
    } else {
        // Lead: longer motif
        motifLen = std::max(motifLen, 4);
    }
    if (motifLen < 1) motifLen = 1;
    if (motifLen > 8) motifLen = 8;

    if (behavior.useMotif) {
        for (int i = 0; i < motifLen; i++) {
            int idx = rand() % scale.count;
            int note = baseRoot + scale.intervals[idx];

            // Bass: less octave jumps
            if (!isBass && behavior.forceOctaveJump && (rand() % 100 < 30)) note += 12;
            if (behavior.allowChromatic && (rand() % 100 < 20)) note += (rand() % 3) - 1;

            motif[i] = note;
        }
    } else {
        // Hypnotic: one note (root biased)
        motif[0] = baseRoot + scale.intervals[0];
        motifLen = 1;
    }

    // 4. Determine target note count (VOICE-DEPENDENT)
    int range = params.maxNotes - params.minNotes;
    int targetNotes = params.minNotes + (range > 0 ? rand() % (range + 1) : 0);
    
    if (isBass) {
        // Bass: fewer notes (anchor role)
        targetNotes = std::min(targetNotes, 6);
    } else {
        // Lead: more notes (melodic role)
        targetNotes = std::max(targetNotes, 8);
    }
    if (targetNotes < 0) targetNotes = 0;
    if (targetNotes > 16) targetNotes = 16;

    // 5. Place notes following stepMask (genre skeleton)
    int placed = 0;
    int lastStep = -10;
    int lastNote = baseRoot;

    // === BASS ANCHOR: Force root notes on steps 0 and 8 FIRST ===
    if (isBass && targetNotes >= 2) {
        // Step 0: always root (downbeat anchor)
        if (stepAllowed(behavior.stepMask, 0)) {
            pattern.steps[0].note = baseRoot;
            pattern.steps[0].velocity = 127;
            pattern.steps[0].accent = true;
            placed++;
            lastStep = 0;
        }
        // Step 8: root on beat 3 (second downbeat anchor)
        if (stepAllowed(behavior.stepMask, 8) && targetNotes >= 2) {
            pattern.steps[8].note = baseRoot;
            pattern.steps[8].velocity = 120;
            pattern.steps[8].accent = true;
            placed++;
        }
    }

    // Fill remaining notes
    for (int step = 0; step < 16 && placed < targetNotes; step++) {
        if (pattern.steps[step].note != -1) continue; // Already placed (anchors)
        if (!stepAllowed(behavior.stepMask, step)) continue;

        // Sparse style: probabilistically skip allowed steps
        bool sparseStyle = (behavior.stepMask == 0x1111) || (behavior.stepMask == 0x0101);
        if (sparseStyle && (rand() % 100 < 45)) continue;

        // Avoid clusters for minimal/hypnotic
        if (behavior.avoidClusters && (step - lastStep) <= 1) continue;

        int note;
        
        if (isBass) {
            // Bass: high root bias, allow repeats
            if (rand() % 100 < prob100(params.rootNoteBias + 0.2f)) {
                note = baseRoot;
            } else {
                note = motif[placed % motifLen];
            }
            // Allow consecutive same notes for bass anchor
        } else {
            // Lead: use motif, less root bias
            note = motif[placed % motifLen];
            if (rand() % 100 < prob100(params.rootNoteBias)) {
                note = baseRoot;
            }
            // Octave variation for lead
            if (rand() % 100 < 25 && octaveRange > 1) {
                note += (rand() % octaveRange) * 12;
            }
        }

        pattern.steps[step].note = note;
        lastNote = note;

        int velRange = std::max(1, params.velocityMax - params.velocityMin + 1);
        pattern.steps[step].velocity = params.velocityMin + (rand() % velRange);

        // Accents: bass on downbeats, lead more varied
        bool isDownbeat = (step % 4 == 0);
        if (isBass) {
            // Bass: accent downbeats
            if (isDownbeat && rand() % 100 < prob100(params.accentProbability + 0.2f)) {
                pattern.steps[step].accent = true;
                pattern.steps[step].velocity = 127;
            }
        } else {
            // Lead: accents more distributed
            if (rand() % 100 < prob100(params.accentProbability)) {
                pattern.steps[step].accent = true;
                pattern.steps[step].velocity = 120;
            }
        }

        // Slides: bass minimal, lead more
        float slideChance = isBass ? params.slideProbability * 0.3f : params.slideProbability;
        if (rand() % 100 < prob100(slideChance)) {
            if (lastStep >= 0 && pattern.steps[lastStep].note != -1 && pattern.steps[lastStep].note != note) {
                if (rand() % 100 < 60) pattern.steps[step].slide = true;
            } else if (!isBass) {
                if (rand() % 100 < 25) pattern.steps[step].slide = true;
            }
        }

        lastStep = step;
        placed++;
    }

    // 6. Ghost notes (lead only)
    if (!isBass && params.ghostProbability > 0.01f) {
        int g = prob100(params.ghostProbability);
        for (int i = 0; i < 16; i++) {
            if (pattern.steps[i].note == -1 && stepAllowed(behavior.stepMask, i)) {
                if (rand() % 100 < g) {
                    pattern.steps[i].note = baseRoot;
                    pattern.steps[i].ghost = true;
                    pattern.steps[i].velocity = 50 + (rand() % 20);
                }
            }
        }
    }

    // 7. Swing + microtiming
    if (params.swingAmount > 0.01f) {
        int8_t swingTicks = (int8_t)(params.swingAmount * 24.0f);
        for (int i = 0; i < 16; i++) {
            if (i % 2 == 1 && pattern.steps[i].note != -1) pattern.steps[i].timing += swingTicks;
        }
    }

    if (params.microTimingAmount > 0.01f) {
        int r = (int)(params.microTimingAmount * 6.0f);
        if (r > 0) {
            for (int i = 0; i < 16; i++) {
                if (pattern.steps[i].note != -1) pattern.steps[i].timing += (rand() % (r * 2 + 1)) - r;
            }
        }
    }
}

// Drum pattern: params-only delegates to behavior version
void GrooveboxModeManager::generateDrumPattern(DrumPatternSet& patternSet, const GenerativeParams& params) const {
    GenreBehavior defaultBehavior = { 0xFFFF, 4, 0, true, true, false, false };
    generateDrumPattern(patternSet, params, defaultBehavior);
}

// Full drum pattern with structural behavior
void GrooveboxModeManager::generateDrumPattern(DrumPatternSet& patternSet, 
                                               const GenerativeParams& params,
                                               const GenreBehavior& behavior) const {
    for (int v = 0; v < DrumPatternSet::kVoices; ++v) {
        generateDrumVoice(patternSet.voices[v], v, params, behavior);
    }
}

void GrooveboxModeManager::generateDrumVoice(DrumPattern& pattern, int voiceIndex,
                                              const GenerativeParams& params,
                                              const GenreBehavior& behavior) const {
    // Clear voice
    for (int s = 0; s < 16; ++s) {
        pattern.steps[s].hit = false;
        pattern.steps[s].accent = false;
        pattern.steps[s].velocity = 100; // FIX: default to 100
        pattern.steps[s].timing = 0;
    }

    // Template detection based on stepMask
    bool hypno = (behavior.stepMask == 0x0101);
    bool minimal = (behavior.stepMask == 0x1111);
    bool electro = (behavior.stepMask == 0xAA55);
    bool rave = (behavior.stepMask == 0xFFFF && behavior.motifLength >= 6);

    switch (voiceIndex) {
        case 0: // KICK
            if (electro) {
                int steps[] = {0, 6, 10, 15};
                for (int i = 0; i < 4; i++) {
                    if (rand() % 100 < 85) {
                        pattern.steps[steps[i]].hit = true;
                        pattern.steps[steps[i]].velocity = 115;
                    }
                }
            } else if (hypno || minimal || params.sparseKick) {
                pattern.steps[0].hit = true;
                pattern.steps[0].velocity = 110;
                if (!hypno && rand() % 100 < 35) {
                    pattern.steps[8].hit = true;
                    pattern.steps[8].velocity = 105;
                }
            } else {
                for (int i = 0; i < 16; i += 4) {
                    pattern.steps[i].hit = true;
                    pattern.steps[i].velocity = rave ? 127 : 112;
                }
                if (rave && rand() % 100 < 45) {
                    pattern.steps[14].hit = true;
                    pattern.steps[14].velocity = 100;
                }
            }
            break;

        case 1: // SNARE/CLAP
            if (!hypno) {
                if (electro) {
                    int s1 = (rand() % 100 < 30) ? 5 : 4;
                    int s2 = (rand() % 100 < 30) ? 13 : 12;
                    pattern.steps[s1].hit = true;
                    pattern.steps[s1].velocity = 110;
                    pattern.steps[s2].hit = true;
                    pattern.steps[s2].velocity = 110;
                } else {
                    pattern.steps[4].hit = true;
                    pattern.steps[4].velocity = 112;
                    pattern.steps[12].hit = true;
                    pattern.steps[12].velocity = 112;
                }
            }
            break;

        case 2: // CLOSED HAT
            if (hypno || minimal || params.sparseHats) {
                for (int i = 2; i < 16; i += 4) {
                    if (rand() % 100 < 70) {
                        pattern.steps[i].hit = true;
                        pattern.steps[i].velocity = 70;
                    }
                }
            } else {
                int step = rave ? 1 : 2;
                for (int i = 0; i < 16; i += step) {
                    if (rand() % 100 < (rave ? 92 : 80)) {
                        pattern.steps[i].hit = true;
                        pattern.steps[i].velocity = (i % 4 == 2) ? 95 : 55;
                    }
                }
            }
            break;

        case 3: // OPEN HAT
            for (int i = 2; i < 16; i += 4) {
                if (rand() % 100 < (minimal ? 30 : 60)) {
                    int pos = i;
                    pattern.steps[pos].hit = true;
                    pattern.steps[pos].velocity = 85;
                }
            }
            break;

        case 4: case 5: case 6: case 7: // PERC/FILLS
            if ((rand() % 100) < prob100(params.fillProbability)) {
                int count = 1 + (rand() % 2);
                for (int i = 0; i < count; i++) {
                    int pos = 12 + (rand() % 4);
                    pattern.steps[pos & 15].hit = true;
                    pattern.steps[pos & 15].velocity = 80 + (i * 10);
                    pattern.steps[pos & 15].accent = (rand() % 100 < 30);
                }
            }
            break;
    }

    // Common: Swing + microtiming
    int8_t swingTicks = (int8_t)(params.swingAmount * 24.0f);
    for (int i = 0; i < 16; ++i) {
        if (i % 2 == 1 && swingTicks > 0) {
            if (pattern.steps[i].hit) {
                pattern.steps[i].timing += swingTicks;
            }
        }
        if (params.microTimingAmount > 0.01f) {
            int r = (int)(params.microTimingAmount * 3.0f);
            if (r > 0) {
                if (pattern.steps[i].hit) {
                    pattern.steps[i].timing += (rand() % (r * 2 + 1)) - r;
                }
            }
        }
    }
}
