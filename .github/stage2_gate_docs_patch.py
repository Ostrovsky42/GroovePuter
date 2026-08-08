from pathlib import Path


def patch(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    if text.count(old) != 1:
        raise SystemExit(f"expected one docs target in {path}, found {text.count(old)}")
    p.write_text(text.replace(old, new, 1), encoding="utf-8")


contracts = "docs/architecture/GROOVE_VOCABULARY_MUSICAL_CONTRACTS.md"
patch(
    contracts,
    """    StepMask preferred;
    StepMask optional;
    StepMask forbidden;
    StepMask protectedSilence;

    uint8_t structuralMin;
""",
    """    StepMask preferred;
    StepMask optional;
    StepMask forbidden;

    // Coordinate-level rhythmic duration policy over declared onset space.
    // Normal is implicit when a realized onset is in none of these masks.
    StepMask shortGate;
    StepMask heldGate;
    StepMask tieGate;

    StepMask protectedSilence;

    uint8_t structuralMin;
""",
)
patch(
    contracts,
    """OptionalEvent
    MAY be selected according to realization budget.
```

An archetype MAY define zero immutable anchors.
""",
    """OptionalEvent
    MAY be selected according to realization budget.

Gate overlays
    shortGate / heldGate / tieGate classify legal onset coordinates.
    MUST be mutually exclusive.
    MUST be subsets of declared legal onset space.
    MUST NOT overlap forbidden/protected silence.
    GateClass::Normal is implicit for every realized onset not covered by an
    explicit gate overlay.
```

An archetype MAY define zero immutable anchors.
""",
)
patch(
    contracts,
    """canonicalAnchors & protectedSilence == 0
structuralMin <= structuralMax
popcount(immutableAnchors) <= structuralMax
```
""",
    """canonicalAnchors & protectedSilence == 0
shortGate & heldGate == 0
shortGate & tieGate == 0
heldGate & tieGate == 0
(shortGate | heldGate | tieGate) & ~declaredOnsetSpace == 0
(shortGate | heldGate | tieGate) & (forbidden | protectedSilence) == 0
structuralMin <= structuralMax
popcount(immutableAnchors) <= structuralMax
```
""",
)
patch(
    contracts,
    """This is architectural pseudocode. The implementation may encode these fields more compactly.

Normative authority:
""",
    """This is architectural pseudocode. The implementation may encode these fields more compactly.

`GateClass::Normal` is the default/implicit class. A lane grammar therefore does
not need a `normalGate` mask; explicit Short/Held/Tie overlays are sufficient and
avoid duplicating the realized onset mask.

Normative authority:
""",
)

brief = "docs/architecture/GROOVE_VOCABULARY_ARCHITECTURE_BRIEF.md"
patch(
    brief,
    """required/forbidden/protected-silence consistency
valid density ranges
""",
    """required/forbidden/protected-silence consistency
gate-mask disjointness and declared-onset containment
valid density ranges
""",
)
patch(
    brief,
    """forbidden zones empty
density legal or explicitly ValidButSparse
same input -> same output
""",
    """forbidden zones empty
explicit Short/Held/Tie gate overlays preserved; Normal remains implicit
density legal or explicitly ValidButSparse
same input -> same output
""",
)
