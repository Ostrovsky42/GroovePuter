#ifndef GROOVEPUTER_GENERATION_HARMONY_GENERATED_HARMONIC_VOCABULARY_H
#define GROOVEPUTER_GENERATION_HARMONY_GENERATED_HARMONIC_VOCABULARY_H

namespace GroovePuterHarmony {

inline constexpr ChordQualityDefinition kBootstrapQualityDefinitions[] = {
    {0, 0x0091u, 3, QualityFamily::MajorMinor, 0x0091u, 1},
    {1, 0x0089u, 3, QualityFamily::MajorMinor, 0x0089u, 1},
    {2, 0x0491u, 4, QualityFamily::Dominant, 0x0091u, 2},
    {3, 0x0049u, 3, QualityFamily::Diminished, 0x0049u, 2},
    {4, 0x0111u, 3, QualityFamily::Augmented, 0x0111u, 3},
    {5, 0x00a1u, 3, QualityFamily::Suspended, 0x00a1u, 1},
    {6, 0x0489u, 4, QualityFamily::MajorMinor, 0x0089u, 2},
    {7, 0x0891u, 4, QualityFamily::MajorMinor, 0x0091u, 2},
    {8, 0x048du, 5, QualityFamily::MajorMinor, 0x0089u, 3},
    {9, 0x0895u, 5, QualityFamily::MajorMinor, 0x0091u, 3},
};

inline constexpr ScaleDefinition kBootstrapScaleDefinitions[] = {
    {0, 0x05adu, 7, ScaleFamily::Minor, 0, 255},
    {1, 0x0ab5u, 7, ScaleFamily::Major, 0, 255},
    {2, 0x06adu, 7, ScaleFamily::Modes, 1, 1},
    {3, 0x05abu, 7, ScaleFamily::Modes, 2, 1},
    {4, 0x0ad5u, 7, ScaleFamily::Modes, 3, 1},
    {5, 0x06b5u, 7, ScaleFamily::Modes, 4, 1},
    {6, 0x056bu, 7, ScaleFamily::Modes, 5, 1},
    {7, 0x0295u, 5, ScaleFamily::Pentatonic, 0, 1},
    {8, 0x04a9u, 5, ScaleFamily::Pentatonic, 1, 0},
    {9, 0x0fffu, 12, ScaleFamily::Chromatic, 0, 255},
};

}  // namespace GroovePuterHarmony

#endif  // GROOVEPUTER_GENERATION_HARMONY_GENERATED_HARMONIC_VOCABULARY_H
