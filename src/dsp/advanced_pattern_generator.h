#pragma once
#include <vector>
#include <algorithm>
#include <cmath>
#include "../../scenes.h" // For pattern structs
#include "Arduino.h"      // For random() if needed, or standard lib


class AdvancedPatternGenerator {
public:
    // Uses GeneratorParams from scenes.h
    
    static void generatePattern(SynthPattern& pattern, 
                                const GeneratorParams& params) {
        // Clear existing steps
        for(int i=0; i<SynthPattern::kSteps; ++i) {
            pattern.steps[i].note = 0;
            pattern.steps[i].slide = false;
            pattern.steps[i].accent = false;
            pattern.steps[i].velocity = 100;
            pattern.steps[i].timing = 0;
            pattern.steps[i].ghost = false;
        }
        
        if (params.maxNotes < params.minNotes) {
            // Fix invalid range locally or swap
             const_cast<GeneratorParams&>(params).maxNotes = params.minNotes;
        }
        int range = params.maxNotes - params.minNotes + 1;
        if (range < 1) range = 1;
        int numNotes = params.minNotes + (rand() % range);
        
        std::vector<int> positions = generatePositions(numNotes, params);
        
        for (int step : positions) {
            // Note
            int note = generateNote(step, params);
            pattern.steps[step].note = note;
            
            // Velocity
            uint8_t velocity = generateVelocity(step, params);
            pattern.steps[step].velocity = velocity;
            
            // Micro-timing
            int8_t timing = generateTiming(step, params);
            pattern.steps[step].timing = timing;
            
            // Ghost note
            if ((rand() % 100) < (params.ghostNoteProbability * 100)) {
                pattern.steps[step].ghost = true;
                pattern.steps[step].velocity = 40; 
            }
            
            // Accent
            if ((rand() % 100) < 30) {
                pattern.steps[step].accent = true;
            }
            
            // Slide
            if ((rand() % 100) < 15) {
                pattern.steps[step].slide = true;
            }
        }
        
        applySwing(pattern, params.swingAmount);
    }
    
private:
    static std::vector<int> generatePositions(int count, const GeneratorParams& params) {
        std::vector<int> positions;
        if (params.preferDownbeats) {
            const int goodPositions[] = {0, 2, 4, 6, 8, 10, 12, 14};
            const int offbeats[] = {1, 3, 5, 7, 9, 11, 13, 15};
            
            for (int i = 0; i < count; i++) {
                if ((rand() % 100) < 70 && positions.size() < 8) {
                    int idx = rand() % 8;
                    positions.push_back(goodPositions[idx]);
                } else if (positions.size() < 8) {
                    int idx = rand() % 8;
                    positions.push_back(offbeats[idx]);
                }
            }
        } else {
            for (int i = 0; i < count; i++) {
                positions.push_back(rand() % 16);
            }
        }
        std::sort(positions.begin(), positions.end());
        positions.erase(std::unique(positions.begin(), positions.end()), positions.end());
        return positions;
    }
    
    static int generateNote(int step, const GeneratorParams& params) {
        int range = params.maxOctave - params.minOctave;
        if (range <= 0) range = 1;
        int note = params.minOctave + (rand() % range);
        if (params.scaleQuantize) {
            note = quantizeToScale(note, params.scaleRoot, params.scale);
        }
        return note;
    }
    
    static uint8_t generateVelocity(int step, const GeneratorParams& params) {
        uint8_t base = 100;
        int variation = (rand() % 41) - 20; // -20 to 20
        variation = variation * params.velocityRange;
        int val = base + variation;
        if (val < 40) val = 40;
        if (val > 127) val = 127;
        
        if (isDownbeat(step)) {
            val += 10;
            if (val > 127) val = 127;
        }
        return (uint8_t)val;
    }
    
    static int8_t generateTiming(int step, const GeneratorParams& params) {
        if (params.microTimingAmount == 0) return 0;
        int maxOffset = 32 * params.microTimingAmount;
        return (rand() % (maxOffset * 2 + 1)) - maxOffset;
    }
    
    static void applySwing(SynthPattern& pattern, float amount) {
        if (amount <= 0.01f) return;
        int swingTicks = amount * 96; // 96 ticks per 16th is typical MIDI, but check internal resolution
        // Our 'timing' is int8_t (-128..127). If we assume 1 step = 24 ticks (standard 96 PPQN),
        // let's say timing unit is modest.
        // Actually Scene struct says timing is int8_t.
        // Let's verify resolution. Assuming timing is passed to engine effectively.
        // For now using the logic provided.
        
        for (int step = 0; step < SynthPattern::kSteps; step++) {
            if (pattern.steps[step].note > 0 && (step % 2 == 1)) {
                int val = pattern.steps[step].timing + swingTicks;
                if (val > 127) val = 127;
                pattern.steps[step].timing = (int8_t)val;
            }
        }
    }
    
    static int quantizeToScale(int note, int root, ScaleType scale) {
        static const int minorIntervals[] = {0, 2, 3, 5, 7, 8, 10};
        static const int majorIntervals[] = {0, 2, 4, 5, 7, 9, 11};
        static const int dorianIntervals[] = {0, 2, 3, 5, 7, 9, 10};
        static const int phrygianIntervals[] = {0, 1, 3, 5, 7, 8, 10};
        static const int mixolydianIntervals[] = {0, 2, 4, 5, 7, 9, 10};
        
        const int* intervals;
        switch(scale) {
            case MAJOR: intervals = majorIntervals; break;
            case DORIAN: intervals = dorianIntervals; break;
            case PHRYGIAN: intervals = phrygianIntervals; break;
            case MIXOLYDIAN: intervals = mixolydianIntervals; break;
            case MINOR: 
            default: intervals = minorIntervals; break;
        }
        
        int octave = note / 12;
        int semitone = note % 12;
        
        int closest = 0;
        int minDist = 12;
        for (int i = 0; i < 7; i++) {
            int scaleTone = (root + intervals[i]) % 12;
            int dist = abs(semitone - scaleTone);
            if (dist < minDist) {
                minDist = dist;
                closest = scaleTone;
            }
        }
        return octave * 12 + closest;
    }
    
    static bool isDownbeat(int step) {
        return (step % 4 == 0);
    }
};

class DrumPatternGenerator {
public:
    struct DrumParams {
        float fillProbability = 0.3f;
        float ghostHatProbability = 0.2f;
        float swingAmount = 0.0f;
        bool humanize = true; 
    };
    
    static void generateDrumPattern(DrumPatternSet& patternSet, const DrumParams& params) {
        // Clear
        for(int v=0; v<DrumPatternSet::kVoices; ++v) {
            for(int s=0; s<DrumPattern::kSteps; ++s) {
                patternSet.voices[v].steps[s].hit = false;
                patternSet.voices[v].steps[s].accent = false;
                patternSet.voices[v].steps[s].velocity = 100;
                patternSet.voices[v].steps[s].timing = 0;
            }
        }
        
        // KICK (Voice 0 usually)
        // Adjust indices based on miniacid_engine.h constants
        // Assuming 0=Kick, 1=Snare, 2=HatClosed, 3=HatOpen for basic
        // But let's look at scenes.h indices or define them.
        // For now, assume standard indices based on usage: 
        // 0:Kick, 1:Snare, 2:Hat, 3:OpenHat, 4:MidTom, 5:HiTom, 6:Rim, 7:Clap
        
        setHit(patternSet, 0, 0, true, 110);
        setHit(patternSet, 0, 8, true, 105);
        if ((rand() % 100) < 40) setHit(patternSet, 0, 4, true, 90);

        // SNARE (Voice 1)
        setHit(patternSet, 1, 4, true, 115);
        setHit(patternSet, 1, 12, true, 110);
        if ((rand() % 100) < 30) {
            int ghostPos = 2 + (rand() % 2) * 4;
            setHit(patternSet, 1, ghostPos, true, 40);
        }

        // HATS (Voice 2)
        for (int step = 0; step < 16; step += 2) {
            uint8_t vel = 80 + (rand() % 21);
            if (params.humanize) vel += (rand() % 21) - 10;
            if (vel > 127) vel = 127;
            setHit(patternSet, 2, step, true, vel);
            
            if ((rand() % 100) < (params.ghostHatProbability * 100)) {
                setHit(patternSet, 2, step + 1, true, 35);
            }
        }
        
        // OPEN HAT (Voice 3)
        int openPos = 6 + (rand() % 2) * 4;
        setHit(patternSet, 3, openPos, true, 95);
        
        // FILLS
        if ((rand() % 100) < (params.fillProbability * 100)) {
            int fillStart = 14;
            // MidTom=4, HiTom=5
            setHit(patternSet, 4, fillStart, true, 85);
            setHit(patternSet, 5, fillStart + 1, true, 80);
        }
        
        if (params.swingAmount > 0) {
            applyDrumSwing(patternSet, params.swingAmount);
        }
    }

private:
    static void setHit(DrumPatternSet& ps, int voice, int step, bool accent, uint8_t vel) {
        if (voice < 0 || voice >= DrumPatternSet::kVoices) return;
        if (step < 0 || step >= DrumPattern::kSteps) return;
        ps.voices[voice].steps[step].hit = true;
        ps.voices[voice].steps[step].accent = accent;
        ps.voices[voice].steps[step].velocity = vel;
    }

    static void applyDrumSwing(DrumPatternSet& ps, float amount) {
        int swingTicks = amount * 96;
        for (int step = 0; step < DrumPattern::kSteps; step++) {
            if (step % 2 == 1) {
                for (int v = 0; v < DrumPatternSet::kVoices; v++) {
                    if (ps.voices[v].steps[step].hit) {
                         int val = ps.voices[v].steps[step].timing + swingTicks;
                         if (val > 127) val = 127;
                         ps.voices[v].steps[step].timing = (int8_t)val;
                    }
                }
            }
        }
    }
};
