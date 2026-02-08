#include "mode_manager.h"
#include "miniacid_engine.h"
#include "groove_profile.h"
#include "../debug_log.h"
#include <algorithm>

void GrooveboxModeManager::setMode(GrooveboxMode mode) {
    currentMode_ = mode;
    engine_.setGrooveboxMode(mode); // Sync to engine
}

void GrooveboxModeManager::toggle() {
    int idx = static_cast<int>(currentMode_);
    idx = (idx + 1) % 5;
    setMode(static_cast<GrooveboxMode>(idx));
}

void GrooveboxModeManager::setFlavor(int flavor) {
    if (flavor < 0) flavor = 0;
    if (flavor >= flavorCount()) flavor = flavorCount() - 1;
    currentFlavor_ = flavor;
}

void GrooveboxModeManager::shiftFlavor(int delta) {
    int v = currentFlavor_ + delta;
    while (v < 0) v += flavorCount();
    while (v >= flavorCount()) v -= flavorCount();
    currentFlavor_ = v;
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
    const ModeConfig& cfg = config();
    
    // 1. Compute interpolation values (BPM & Flavor)
    float t = (bpm - 80.0f) / (170.0f - 80.0f);
    if (t < 0.0f) t = 0.0f; else if (t > 1.0f) t = 1.0f;
    
    auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
    auto prob100 = [](float p){ return int(p * 100.0f + 0.5f); };

    PatternCorridors corridors = GrooveProfile::getCorridors(currentMode_, currentFlavor_);
    float flavorAccentProb = corridors.accentProbability;
    float flavorSlideProb = corridors.slideProbability;
    float flavorSwing = corridors.swingAmount;
    int adaptedMinNotes = corridors.notesMin;
    int adaptedMaxNotes = corridors.notesMax;

    // BPM adaptation: Fewer notes at higher BPM
    adaptedMinNotes = (int)lerp((float)adaptedMinNotes * 1.05f, (float)adaptedMinNotes * 0.85f, t);
    adaptedMaxNotes = (int)lerp((float)adaptedMaxNotes * 1.05f, (float)adaptedMaxNotes * 0.85f, t);

    float delayMix = std::max(engine_.tempoDelay(0).mixValue(), engine_.tempoDelay(1).mixValue());
    float tapeSpace = engine_.sceneManager().currentScene().tape.space / 100.0f;
    GrooveProfile::applyBudgetRules(currentMode_, delayMix, tapeSpace, corridors);
    adaptedMinNotes = corridors.notesMin;
    adaptedMaxNotes = corridors.notesMax;
    flavorAccentProb = corridors.accentProbability;
    flavorSlideProb = corridors.slideProbability;
    flavorSwing = corridors.swingAmount;

    // Genre Anchors & Global Contexts
    float adaptedChromaticProb = cfg.pattern.chromaticProbability;
    float adaptedGhostProb = cfg.pattern.ghostProbability;
    float adaptedSwing = flavorSwing;
    bool staccato = false;

    if (currentMode_ == GrooveboxMode::Acid) {
        adaptedChromaticProb = lerp(0.18f, 0.05f, t);
        adaptedGhostProb = lerp(0.12f, 0.04f, t);
    } else if (currentMode_ == GrooveboxMode::Breaks) {
        adaptedSwing = flavorSwing;
        adaptedGhostProb = lerp(0.28f, 0.12f, t);
    } else if (currentMode_ == GrooveboxMode::Electro) {
        flavorSlideProb = corridors.slideProbability;
        adaptedSwing = flavorSwing;
        staccato = true;
        adaptedGhostProb = 0.12f;
    } else if (currentMode_ == GrooveboxMode::Dub) {
        adaptedGhostProb = lerp(0.38f, 0.18f, t);
    }

    // 2. Budgeting (Strict Density)
    int targetNotes = adaptedMinNotes + (rand() % (adaptedMaxNotes - adaptedMinNotes + 1));
    if (targetNotes < 1) targetNotes = 1;
    if (targetNotes > 16) targetNotes = 16;
    
    int targetGhosts = (int)(adaptedGhostProb * (16 - targetNotes));

    // 3. Clear Pattern
    for (int i = 0; i < 16; i++) {
        pattern.steps[i].note = -1;
        pattern.steps[i].accent = false;
        pattern.steps[i].slide = false;
        pattern.steps[i].ghost = false;
        pattern.steps[i].velocity = 100;
        pattern.steps[i].timing = 0;
    }

    // Determine scale and root
    const bool isStrictScale = (currentMode_ == GrooveboxMode::Acid || currentMode_ == GrooveboxMode::Electro);
    const Scale& scale = isStrictScale ? kScales[0] : kScales[rand() % 4];
    int rootNote = isStrictScale ? 36 : 24;

    // 4. Placement (Strict Density Shuffle)
    int indices[16];
    for (int i = 0; i < 16; i++) indices[i] = i;
    for (int i = 0; i < 16; i++) {
        int r = rand() % 16;
        int tmp = indices[i]; indices[i] = indices[r]; indices[r] = tmp;
    }

    int placed = 0;
    
    // Genre specific anchors first
    if (currentMode_ == GrooveboxMode::Minimal || currentMode_ == GrooveboxMode::Dub) {
        static const int anchors[] = {0, 8, 4, 12};
        for (int a : anchors) {
            if (placed < targetNotes && (rand() % 100 < 80)) {
                pattern.steps[a].note = rootNote;
                pattern.steps[a].velocity = 120;
                pattern.steps[a].accent = true;
                placed++;
            }
        }
    }

    // Random placement from shuffled indices
    for (int pIdx = 0; pIdx < 16 && placed < targetNotes; pIdx++) {
        int i = indices[pIdx];
        if (pattern.steps[i].note != -1) continue;

        int scaleIdx = rand() % scale.count;
        int note = rootNote + scale.intervals[scaleIdx];
        
        // Melodic refinements
        if ((rand() % 100) < prob100(adaptedChromaticProb)) note += (rand() % 3) - 1;
        if (rand() % 100 < 15) note += 12;

        pattern.steps[i].note = note;
        pattern.steps[i].velocity = staccato ? 95 : 100 + (rand() % 20);
        
        if ((rand() % 100) < prob100(flavorAccentProb)) {
            pattern.steps[i].accent = true;
            pattern.steps[i].velocity = 127;
        }

        if (!staccato && (rand() % 100) < prob100(flavorSlideProb)) {
            pattern.steps[i].slide = true;
            if (rand() % 100 < 40) pattern.steps[i].accent = true; // Slide-accents are very Acid
        }
        
        placed++;
    }

    // Place textual ghosts in remaining empty slots
    for (int pIdx = 0; pIdx < 16 && targetGhosts > 0; pIdx++) {
        int i = indices[pIdx];
        if (pattern.steps[i].note != -1) continue;
        
        pattern.steps[i].note = rootNote;
        pattern.steps[i].ghost = true;
        pattern.steps[i].velocity = 40 + (rand() % 20);
        targetGhosts--;
    }

    // 5. Apply Groove (Swing)
    if (adaptedSwing > 0.01f) {
        for (int i = 1; i < 16; i += 2) {
            pattern.steps[i].timing = (int8_t)(adaptedSwing * 36.0f);
        }
    }

    const PatternMetrics m = GrooveProfile::analyzePattern(pattern);
    LOG_DEBUG_PATTERN("mode=%d flv=%d notes=%d rests=%d acc=%d sl=%d swing=%.2f",
                      (int)currentMode_, currentFlavor_, m.notes, m.rests, m.accents, m.slides, adaptedSwing);
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

    const PatternMetrics m = GrooveProfile::analyzePattern(pattern);
    LOG_DEBUG_PATTERN("voice=%d notes=%d rests=%d acc=%d sl=%d swing=%.2f",
                      voiceIndex, m.notes, m.rests, m.accents, m.slides, params.swingAmount);
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
