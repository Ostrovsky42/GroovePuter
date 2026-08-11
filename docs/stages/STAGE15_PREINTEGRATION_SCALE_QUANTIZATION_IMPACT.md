# Audible impact note

This correctness gate intentionally changes generated synth-note quantization for exactly these `ScaleType` selections in the legacy `AdvancedPatternGenerator` path:

- `PENTATONIC_MJ`: previously aliased to MINOR through `% 7`; now uses major pentatonic `{0,2,4,7,9}`.
- `PENTATONIC_MN`: previously aliased to MAJOR through `% 7`; now uses minor pentatonic `{0,3,5,7,10}`.
- `CHROMATIC`: previously aliased to DORIAN through `% 7`; now passes all twelve pitch classes.

The seven existing modal values `MINOR..LOCRIAN` retain the same interval definitions and nearest-tone algorithm.

Scene impact inventory: this PR does not claim a static list of affected saved scenes because persisted `GeneratorParams.scale` can vary per scene/project. The affected set is therefore exactly any legacy AdvancedPatternGenerator invocation whose runtime `params.scale` is one of the three values above. No Scene schema or persistence representation changes.
