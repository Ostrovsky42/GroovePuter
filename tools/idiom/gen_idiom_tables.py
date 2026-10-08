#!/usr/bin/env python3
"""Emit src/generation/idiom/genre_idiom_tables.h from the owner's SEQTRAK genre
dataset (track_patterns.csv) plus the hand-written House idiom below.

Usage: tools/idiom/gen_idiom_tables.py <path/to/track_patterns.csv> > src/generation/idiom/genre_idiom_tables.h

Pitches become semitones: bass relative to C2, melody relative to C4; chords
become a root (relative to C) and an interval mask. The dataset is in C minor;
GenreIdiom maps every semitone onto the project key and scale at runtime.
"""
import csv
import sys

VARIANTS = [
    "acid_chicago_jack", "acid_rolling",
    "lofi_classic_chill", "lofi_drunken",
    "boombap_golden_era", "boombap_dusty_jazz",
    "ukg_classic_2step", "ukg_dark_skippy",
    "dub_deep_chord", "dub_minimal_space",
    "jungle_classic_amen", "jungle_atmospheric",
]

NOTE_PC = {"C": 0, "Db": 1, "D": 2, "Eb": 3, "E": 4, "F": 5, "Gb": 6, "G": 7,
           "Ab": 8, "A": 9, "Bb": 10, "B": 11}

CHORD_INTERVALS = {
    "m": [0, 3, 7], "m7": [0, 3, 7, 10], "m9": [0, 3, 7, 10, 14],
    "m11": [0, 3, 7, 10, 14, 17], "maj7": [0, 4, 7, 11], "": [0, 4, 7],
    "6": [0, 4, 7, 9], "7alt": [0, 4, 10, 13], "9": [0, 4, 7, 10, 14],
    "maj9": [0, 4, 7, 11, 14], "7": [0, 4, 7, 10], "sus4": [0, 5, 7],
}

# Hand-written House (not in the dataset): offbeat bass with an octave pop,
# minor-9 stabs between kick and bass, a pickup melody. 1-based steps like the
# CSV. Research: docs/superpowers/plans/2026-10-08-0918-genre-idioms-research.md
HOUSE = {"progression": (0, 0, 0, 0),
    "P1": {"SYNTH1": ("3|7|11|15", "C2|C2|C3|C2", "1|1|1|1", "104|92|98|90", ""),
           "SYNTH2": ("4|8|12", "Cm9|Cm9|Fm9", "1|2|2", "78|72|80"),
           "DX": ("16", "Bb4", "1", "64")},
    "P2": {"SYNTH1": ("3|7|8|11|15|16", "C2|C2|Bb1|C3|C2|G1", "1|1|1|1|1|1",
                      "104|92|70|98|90|72", "11:accent=1"),
           "SYNTH2": ("4|8|12|15", "Cm9|Cm9|Fm9|Fm9", "1|2|2|1", "78|72|80|66"),
           "DX": ("6|16", "G4|Bb4", "2|1", "60|66")},
    "P3": {"SYNTH1": ("3|11", "C2|C3", "2|2", "100|94", ""),
           "SYNTH2": ("4|12", "Cm9|Fm9", "3|3", "76|78"),
           "DX": ("", "", "", "")},
}



# Hand-written genres the dataset does not cover. Each has a 4-bar
# progression in scale degrees (0 = tonic): the reference corpus (docs/midi,
# Hotline Miami OST) moves the bass with the chord bar by bar (A a A a) instead
# of writing new bars. "" = no part.
def level(bass, stab, melody):
    return {"SYNTH1": bass, "SYNTH2": stab, "DX": melody}

NONE4 = ("", "", "", "")
HAND = {
    # Synthwave: steady octave-pumping eighths, long wide melody notes, i-VI-III-VII.
    "outrun_drive": {"progression": (0, 5, 2, 6),
        "P1": level(("1|3|5|7|9|11|13|15", "C2|C2|C3|C2|C2|C2|C3|C2", "1|1|1|1|1|1|1|1",
                     "100|80|96|78|98|80|96|82", ""), NONE4,
                    ("1|7|9|15", "G4|Eb5|D5|C5", "6|2|6|2", "84|76|82|74")),
        "P2": level(("1|3|4|5|7|9|11|12|13|15", "C2|C2|C3|C2|C2|C2|C2|C3|C2|C2",
                     "1|1|1|1|1|1|1|1|1|1", "100|80|90|96|78|98|80|88|96|82", ""), NONE4,
                    ("1|5|7|9|13|15", "G4|F4|Eb5|D5|C5|Bb4", "4|2|2|4|2|2", "84|70|78|82|76|72")),
        "P3": level(("1|5|9|13", "C2|C2|C2|C2", "2|2|2|2", "96|84|92|84", ""), NONE4,
                    ("1|9", "G4|Eb5", "8|8", "80|78"))},
    # Darksynth: driving sixteenths with 3-3-2 accents, a b2 tension note, a
    # short menacing riff; i-i-VI-VII.
    "darksynth_drive": {"progression": (0, 0, 5, 6),
        "P1": level(("1|2|3|4|5|6|7|8|9|10|11|12|13|14|15|16",
                     "C2|C2|C2|C2|C2|C2|C2|C2|C3|C2|C2|Db2|C2|C2|C3|C2",
                     "1|1|1|1|1|1|1|1|1|1|1|1|1|1|1|1",
                     "112|70|74|100|72|70|104|72|108|70|74|96|100|72|106|74",
                     "1:accent=1|4:accent=1|7:accent=1|9:accent=1"), NONE4,
                    ("1|4|7|9|12|15", "C4|Eb4|C4|Db4|C4|G4", "2|2|1|2|2|1", "96|84|80|92|84|88")),
        "P2": level(("1|2|3|4|5|6|7|8|9|10|11|12|13|14|15|16",
                     "C2|C2|C2|C2|C2|C2|C2|C2|C3|C2|C2|Db2|C2|C2|C3|C2",
                     "1|1|1|1|1|1|1|1|1|1|1|1|1|1|1|1",
                     "112|70|74|100|72|70|104|72|108|70|74|96|100|72|106|74",
                     "1:accent=1|4:accent=1|7:accent=1|9:accent=1"), NONE4,
                    ("1|4|7|9|12|15|16", "C4|Eb4|C4|Db4|C4|G4|Bb4", "2|2|1|2|2|1|1",
                     "96|84|80|92|84|88|76")),
        "P3": level(("1|3|5|7|9|11|13|15", "C2|C2|C2|C2|C3|C2|C2|C2", "1|1|1|1|1|1|1|1",
                     "104|76|96|76|100|76|96|76", ""), NONE4,
                    ("1|9", "C4|Eb4", "6|6", "88|84"))},
    # Funk/soul: two-handed bass (octave pops, a ghost), clav stabs in its gaps; i-iv.
    "funk_pocket": {"progression": (0, 0, 3, 3),
        "P1": level(("1|4|7|8|11|14|15", "C2|C2|C3|Bb2|C2|G2|C3", "2|1|1|1|2|1|1",
                     "104|48|96|84|98|86|90", "7:accent=1"),
                    ("2|5|10|13", "Cm7|Cm7|Cm7|Cm7", "1|1|1|1", "70|76|70|78"),
                    ("16", "Bb4", "1", "70")),
        "P2": level(("1|4|7|8|9|11|12|14|15", "C2|C2|C3|Bb2|Eb2|C2|F2|G2|C3",
                     "2|1|1|1|1|1|1|1|1", "104|48|96|84|70|98|72|86|90", "7:accent=1"),
                    ("2|5|10|13|16", "Cm7|Cm7|Cm7|Cm7|Fm7", "1|1|1|1|1", "70|76|70|78|66"),
                    ("8|16", "G4|Bb4", "1|1", "68|72")),
        "P3": level(("1|7|11|15", "C2|C3|C2|G2", "2|1|2|1", "100|92|94|84", ""),
                    ("5|13", "Cm7|Cm7", "2|2", "72|74"), NONE4)},
    # Electro: syncopated 808-style bass, robotic off-beat lead; i-i-VII-VI.
    "electro_machine": {"progression": (0, 0, 6, 5),
        "P1": level(("1|4|7|11|13", "C2|C3|C2|Bb1|C2", "1|1|1|1|1", "108|90|96|86|100", ""), NONE4,
                    ("3|7|11|15", "G4|C5|G4|Eb5", "1|1|1|1", "80|84|78|86")),
        "P2": level(("1|4|7|11|13|15", "C2|C3|C2|Bb1|C2|G1", "1|1|1|1|1|1",
                     "108|90|96|86|100|80", ""), NONE4,
                    ("3|5|7|11|15", "G4|Bb4|C5|G4|Eb5", "1|1|1|1|1", "80|70|84|78|86")),
        "P3": level(("1|7|11", "C2|C2|Bb1", "1|1|1", "104|92|86", ""), NONE4,
                    ("7|15", "C5|Eb5", "1|1", "82|84"))},
    # Broken beat: bass between the kicks, minor-9 chords, a sparse answer; i-iv-i-v.
    "broken_bruk": {"progression": (0, 3, 0, 4),
        "P1": level(("1|6|9|12|14", "C2|G1|Bb1|C2|Eb2", "2|1|2|1|2", "102|84|92|80|88", ""),
                    ("3|11", "Cm9|Cm9", "2|2", "76|72"),
                    ("8|16", "G4|Bb4", "1|1", "72|70")),
        "P2": level(("1|6|9|12|14|16", "C2|G1|Bb1|C2|Eb2|F2", "2|1|2|1|2|1",
                     "102|84|92|80|88|72", ""),
                    ("3|11", "Cm9|Cm9", "2|2", "76|72"),
                    ("5|8|16", "Eb5|G4|Bb4", "1|1|1", "66|72|70")),
        "P3": level(("1|9", "C2|Bb1", "3|3", "98|88", ""),
                    ("3", "Cm9", "4", "74"), ("8", "G4", "1", "70"))},
    # Chiptune: octave bass in eighths, the fast arpeggio as the lead; i-VI-VII-i.
    "chip_arp": {"progression": (0, 5, 6, 0),
        "P1": level(("1|3|5|7|9|11|13|15", "C2|C3|C2|C3|C2|C3|C2|C3", "1|1|1|1|1|1|1|1",
                     "100|80|96|80|98|80|96|80", ""), NONE4,
                    ("1|2|3|4|5|6|7|8|9|10|11|12|13|14|15|16",
                     "C5|Eb5|G5|C5|Eb5|G5|C5|Eb5|G5|C5|Eb5|G5|C5|Eb5|G5|C6",
                     "1|1|1|1|1|1|1|1|1|1|1|1|1|1|1|1",
                     "86|70|74|84|70|74|84|70|74|84|70|74|84|70|74|80")),
        "P2": level(("1|3|5|7|9|11|13|15", "C2|C3|C2|C3|C2|C3|C2|C3", "1|1|1|1|1|1|1|1",
                     "100|80|96|80|98|80|96|80", ""), NONE4,
                    ("1|2|3|4|5|6|7|8|9|10|11|12|13|14|15|16",
                     "C5|Eb5|G5|C6|C5|Eb5|G5|C6|C5|Eb5|G5|C6|C5|Eb5|G5|C6",
                     "1|1|1|1|1|1|1|1|1|1|1|1|1|1|1|1",
                     "86|70|74|80|84|70|74|80|84|70|74|80|84|70|74|80")),
        "P3": level(("1|5|9|13", "C2|C2|C2|C2", "1|1|1|1", "98|90|96|90", ""), NONE4,
                    ("1|3|5|7|9|11|13|15", "C5|Eb5|G5|C6|G5|Eb5|C5|G4", "1|1|1|1|1|1|1|1",
                     "84|72|76|82|76|72|80|70"))},
}

def pitch_semi(name, base_octave):
    i = 1
    if len(name) > 1 and name[1] in "b#":
        i = 2
    pc = NOTE_PC[name[:i].replace("#", "")] + (1 if "#" in name[:i] else 0)
    octave = int(name[i:])
    return pc + 12 * (octave - base_octave)


def chord(name):
    name = name.split("/")[0]
    i = 1
    if len(name) > 1 and name[1] in "b#":
        i = 2
    root = NOTE_PC[name[:i]]
    quality = name[i:]
    mask = 0
    for interval in CHORD_INTERVALS[quality]:
        mask |= 1 << interval
    return root, mask


def split(field):
    return [x for x in field.split("|") if x != ""] if field else []


def locks(field):
    out = {}
    for part in split(field):
        step, _, rest = part.partition(":")
        flags = 0
        for item in rest.split(";"):
            if item == "accent=1":
                flags |= 1
            if item == "slide=1":
                flags |= 2
        out[int(step)] = flags
    return out


def notes(steps, pitches, lengths, velocities, lock_field, base_octave):
    lk = locks(lock_field)
    rows = []
    for s, p, l, v in zip(split(steps), split(pitches), split(lengths), split(velocities)):
        rows.append("{%d, %d, %d, %d, %d}" % (int(s) - 1, pitch_semi(p, base_octave),
                                              int(l), int(v), lk.get(int(s), 0)))
    return rows


def stabs(steps, chords, lengths, velocities):
    rows = []
    for s, c, l, v in zip(split(steps), split(chords), split(lengths), split(velocities)):
        root, mask = chord(c)
        rows.append("{%d, %d, 0x%Xu, %d, %d}" % (int(s) - 1, root, mask, int(l), int(v)))
    return rows


def emit_level(name, level, data):
    b = notes(*data["SYNTH1"], 2)
    s = stabs(*data["SYNTH2"])
    m = notes(*data["DX"], "", 4)
    out = []
    for kind, rows, typ in (("bass", b, "IdiomNote"), ("stab", s, "IdiomStab"),
                            ("melody", m, "IdiomNote")):
        if rows:
            out.append("inline constexpr %s k_%s_%s_%s[] = {%s};" % (
                typ, name, level, kind, ", ".join(rows)))
    ref = []
    for kind, rows in (("bass", b), ("stab", s), ("melody", m)):
        ref.append(("k_%s_%s_%s" % (name, level, kind), len(rows)) if rows else ("nullptr", 0))
    return out, "{%s, %d, %s, %d, %s, %d}" % (ref[0][0], ref[0][1], ref[1][0], ref[1][1],
                                              ref[2][0], ref[2][1])


def main():
    rows = {}
    with open(sys.argv[1], encoding="utf-8-sig") as handle:
        for r in csv.DictReader(handle):
            if r["variant_id"] in VARIANTS and r["track_id"] in ("SYNTH1", "SYNTH2", "DX"):
                tr = r["track_id"]
                fields = (r["active_steps"], r["pitches"], r["note_lengths_steps"], r["velocities"])
                if tr == "SYNTH1":
                    fields = fields + (r["parameter_locks"],)
                rows.setdefault(r["variant_id"], {}).setdefault(r["pattern_id"], {})[tr] = fields
    rows["house_deep_offbeat"] = HOUSE
    rows.update(HAND)
    for name in VARIANTS:
        rows[name]["progression"] = (0, 0, 0, 0)
    guard = "GROOVEPUTER_GENERATION_IDIOM_GENRE_IDIOM_TABLES_H"
    print("#ifndef %s\n#define %s\n" % (guard, guard))
    print("// Generated by tools/idiom/gen_idiom_tables.py from the SEQTRAK genre")
    print("// dataset (track_patterns.csv) + hand-written House. Do not edit by hand.")
    print("\n#include \"genre_idiom_types.h\"\n\nnamespace GenreIdiom {\n")
    variants = []
    names = VARIANTS + ["house_deep_offbeat"] + list(HAND)
    for name in names:
        levels = []
        for level in ("P1", "P2", "P3"):
            data = rows[name][level]
            decls, ref = emit_level(name, level, data)
            print("\n".join(decls))
            levels.append(ref)
        variants.append((name, levels, rows[name]["progression"]))
    print()
    for name, levels, progression in variants:
        print("inline constexpr IdiomVariant k_%s = {\"%s\", {%s}, {%s}};" % (
            name, name, ", ".join(levels), ", ".join(str(d) for d in progression)))
    print("\n}  // namespace GenreIdiom")
    print("\n#endif  // %s" % guard)


if __name__ == "__main__":
    main()
