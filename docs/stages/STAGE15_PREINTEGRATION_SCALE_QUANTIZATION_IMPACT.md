# Audible impact note

This correctness gate intentionally changes generated synth-note quantization for exactly these `ScaleType` selections in the legacy `AdvancedPatternGenerator` path:

- `PENTATONIC_MJ`: previously aliased to MINOR through `% 7`; now uses major pentatonic `{0,2,4,7,9}`.
- `PENTATONIC_MN`: previously aliased to MAJOR through `% 7`; now uses minor pentatonic `{0,3,5,7,10}`.
- `CHROMATIC`: previously aliased to DORIAN through `% 7`; now passes all twelve pitch classes.

The seven existing modal values `MINOR..LOCRIAN` retain the same interval definitions and nearest-tone algorithm. The focused regression compares their exact legacy output across roots C/D/B and MIDI notes 36..95.

## Scene / code impact inventory

Repository code search for the three affected enum names found only the `ScaleType` declaration and scale-selection UI references; no hard-coded scene/preset assignment to `PENTATONIC_MJ`, `PENTATONIC_MN`, or `CHROMATIC` was found.

Saved projects can still persist `GeneratorParams.scale`, so a complete static list of user scenes cannot be derived from repository source. The affected runtime set is exactly any legacy `AdvancedPatternGenerator` invocation whose `params.scale` is one of the three values above.

No Scene schema or persistence representation changes.
