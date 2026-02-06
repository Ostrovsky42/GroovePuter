#pragma once

#include <stdint.h>
#include <cstring>

/**
 * Text-to-Phoneme Converter
 * 
 * Simple rule-based conversion from English text to phoneme sequences.
 * Designed to be compact and handle common GroovePuter vocabulary.
 * 
 * Phoneme symbols used:
 *   Vowels: a(ah), e(eh), i(ee), o(oh), u(oo), @(schwa), A(ae), O(aw)
 *   Consonants: s, z, f, v, t, d, k, g, n, m, l, r, p, b, w, y, h
 *   Special: ' ' (silence/pause)
 */

constexpr int MAX_PHONEME_OUTPUT = 128;

class TextToPhoneme {
public:
    /**
     * Convert text to phoneme string.
     * Output is placed in the provided buffer.
     * Returns number of phonemes written.
     */
    static int convert(const char* text, char* phonemeOut, int maxLen);
    
    /**
     * Get phoneme representation for a single word.
     * Returns nullptr if word not in dictionary.
     */
    static const char* lookupWord(const char* word);
    
private:
    struct WordEntry {
        const char* word;
        const char* phonemes;
    };
    
    // Dictionary of common GroovePuter words
    static const WordEntry DICTIONARY[];
    static const int DICTIONARY_SIZE;
    
    // Letter-to-phoneme rules for unknown words
    static char letterToPhoneme(char c, char prev, char next);
};

// ─────────────────────────────────────────────────────────────
// Implementation (header-only for simplicity)
// ─────────────────────────────────────────────────────────────

// Common words dictionary optimized for GroovePuter
inline const TextToPhoneme::WordEntry TextToPhoneme::DICTIONARY[] = {
    // Numbers
    {"0", "ziro"},
    {"1", "wun"},
    {"2", "tu"},
    {"3", "Tri"},
    {"4", "fOr"},
    {"5", "faiv"},
    {"6", "siks"},
    {"7", "sevn"},
    {"8", "eit"},
    {"9", "nain"},
    {"10", "ten"},
    
    // Music terms
    {"acid", "Asid"},
    {"bass", "beis"},
    {"beat", "bit"},
    {"bpm", "bi pi em"},
    {"drum", "drum"},
    {"drums", "drumz"},
    {"filter", "filt@r"},
    {"kick", "kik"},
    {"mode", "mod"},
    {"mute", "myut"},
    {"pattern", "pAt@rn"},
    {"play", "plei"},
    {"project", "pradjekt"},
    {"resonance", "rez@n@ns"},
    {"sample", "sAmpl"},
    {"scene", "sin"},
    {"sequencer", "sikwens@r"},
    {"snare", "sner"},
    {"song", "sOng"},
    {"stop", "stap"},
    {"synth", "sinT"},
    {"techno", "tekno"},
    {"tempo", "tempo"},
    {"track", "trAk"},
    {"voice", "vois"},
    {"waveform", "weivfOrm"},
    
    // UI/Status words
    {"error", "er@r"},
    {"loading", "lodiN"},
    {"ready", "redi"},
    {"recording", "rek@rdiN"},
    {"saved", "seivd"},
    {"saving", "seiviN"},
    
    // Common words
    {"a", "@"},
    {"and", "And"},
    {"go", "go"},
    {"hi", "hai"},
    {"is", "iz"},
    {"no", "no"},
    {"off", "Of"},
    {"ok", "okei"},
    {"on", "an"},
    {"one", "wun"},
    {"the", "D@"},
    {"to", "tu"},
    {"two", "tu"},
    {"yes", "yes"},
    
    // Genre names
    {"minimal", "minim@l"},
    {"house", "haus"},
    {"trance", "trAns"},
    {"electro", "elektro"},
    {"industrial", "indastri@l"},
    
    // Phrase templates
    {"hello", "helo"},
    {"goodbye", "gudbai"},
    {"welcome", "welk@m"},
};

inline const int TextToPhoneme::DICTIONARY_SIZE = sizeof(DICTIONARY) / sizeof(DICTIONARY[0]);

inline const char* TextToPhoneme::lookupWord(const char* word) {
    if (!word || word[0] == '\0') return nullptr;
    
    // Make lowercase copy for comparison
    char lower[32];
    int i = 0;
    while (word[i] && i < 31) {
        char c = word[i];
        if (c >= 'A' && c <= 'Z') c += 32;
        lower[i] = c;
        i++;
    }
    lower[i] = '\0';
    
    // Search dictionary
    for (int j = 0; j < DICTIONARY_SIZE; j++) {
        if (strcmp(lower, DICTIONARY[j].word) == 0) {
            return DICTIONARY[j].phonemes;
        }
    }
    
    return nullptr;
}

inline char TextToPhoneme::letterToPhoneme(char c, char prev, char next) {
    // Convert to lowercase
    if (c >= 'A' && c <= 'Z') c += 32;
    
    // Vowels
    switch (c) {
        case 'a':
            if (next == 'i' || next == 'y') return 'e'; // "ai", "ay" -> ei
            if (next == 'e') return 'e'; // "ae" -> ei (make, cake)
            return 'A'; // default "a" sound
        case 'e':
            if (prev == 'a' || prev == 'i' || prev == 'o' || prev == 'u') return '\0'; // silent e
            if (next == 'e') return 'i'; // "ee" -> i
            return 'e';
        case 'i':
            if (next == 'e' || next == 'y') return 'i'; // "ie", "iy" -> i
            return 'i';
        case 'o':
            if (next == 'o') return 'u'; // "oo" -> u
            if (next == 'u' || next == 'w') return 'a'; // "ou", "ow" -> au
            return 'o';
        case 'u':
            return 'u';
        case 'y':
            // Y as vowel at end of word
            if (next == '\0' || next == ' ') return 'i';
            return 'y';
            
        // Consonants - mostly direct mapping
        case 'b': return 'b';
        case 'c':
            if (next == 'h') return '\0'; // handled as digraph
            if (next == 'e' || next == 'i' || next == 'y') return 's'; // soft c
            return 'k'; // hard c
        case 'd': return 'd';
        case 'f': return 'f';
        case 'g':
            if (next == 'h') return '\0'; // silent in "gh"
            return 'g';
        case 'h':
            if (prev == 'c' || prev == 's' || prev == 't' || prev == 'g') return '\0'; // part of digraph
            return 'h';
        case 'j': return 'd'; // approximate j as "dz"
        case 'k': return 'k';
        case 'l': return 'l';
        case 'm': return 'm';
        case 'n': return 'n';
        case 'p': return 'p';
        case 'q': return 'k';
        case 'r': return 'r';
        case 's':
            if (next == 'h') return '\0'; // handled as digraph
            return 's';
        case 't':
            if (next == 'h') return '\0'; // handled as digraph
            return 't';
        case 'v': return 'v';
        case 'w': return 'w';
        case 'x': return 'k'; // approximate
        case 'z': return 'z';
        
        // Punctuation / space
        case ' ':
        case ',':
        case '.':
        case '!':
        case '?':
        case ':':
            return ' ';
            
        default:
            return '\0'; // skip unknown
    }
}

inline int TextToPhoneme::convert(const char* text, char* phonemeOut, int maxLen) {
    if (!text || !phonemeOut || maxLen <= 0) return 0;
    
    int outPos = 0;
    int textLen = strlen(text);
    
    // Process word by word
    int wordStart = 0;
    
    for (int i = 0; i <= textLen; i++) {
        char c = text[i];
        bool isWordEnd = (c == ' ' || c == '\0' || c == ',' || c == '.' || c == '!' || c == '?');
        
        if (isWordEnd && i > wordStart) {
            // Extract word
            char word[32];
            int wordLen = i - wordStart;
            if (wordLen > 31) wordLen = 31;
            memcpy(word, text + wordStart, wordLen);
            word[wordLen] = '\0';
            
            // Try dictionary lookup first
            const char* phonemes = lookupWord(word);
            
            if (phonemes) {
                // Copy dictionary phonemes
                int pLen = strlen(phonemes);
                for (int j = 0; j < pLen && outPos < maxLen - 1; j++) {
                    phonemeOut[outPos++] = phonemes[j];
                }
            } else {
                // Fall back to letter-by-letter conversion
                for (int j = 0; j < wordLen && outPos < maxLen - 1; j++) {
                    char prev = (j > 0) ? word[j-1] : '\0';
                    char next = word[j+1]; // safe, word is null-terminated
                    char p = letterToPhoneme(word[j], prev, next);
                    if (p != '\0') {
                        phonemeOut[outPos++] = p;
                    }
                }
            }
            
            // Add space after word
            if (c == ' ' && outPos < maxLen - 1) {
                phonemeOut[outPos++] = ' ';
            }
            
            wordStart = i + 1;
        }
        
        // Handle end of string
        if (c == '\0') break;
        
        // Skip to next word on punctuation
        if (c == ',' || c == '.' || c == '!' || c == '?') {
            if (outPos < maxLen - 1) {
                phonemeOut[outPos++] = ' ';
            }
            wordStart = i + 1;
        }
    }
    
    phonemeOut[outPos] = '\0';
    return outPos;
}
