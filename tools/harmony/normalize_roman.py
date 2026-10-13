#!/usr/bin/env python3
"""Normalize Roman routes using the pinned Mingus 0.6.1 resolver contract."""

import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
QUALITY_REGISTRY_PATH = ROOT / "tools/harmony/quality_registry.json"

_TOKEN = re.compile(r"^(?P<acc>[b#]*)(?P<degree>VII|VI|IV|V|III|II|I)(?P<suffix>.*)$",
                    re.IGNORECASE)
_ABSOLUTE_TOKEN = re.compile(r"^(?P<root>[A-Ga-g](?:[#b]{0,2}))(?P<suffix>.*)$")
_PITCH_CLASS = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}
_MAJOR_ROOTS = (0, 2, 4, 5, 7, 9, 11)
_MINOR_ROOTS = (0, 2, 3, 5, 7, 8, 10)
_MAJOR_TRIADS = ((0, 4, 7), (0, 3, 7), (0, 3, 7), (0, 4, 7),
                 (0, 4, 7), (0, 3, 7), (0, 3, 6))
_MINOR_TRIADS = ((0, 3, 7), (0, 3, 6), (0, 4, 7), (0, 3, 7),
                 (0, 3, 7), (0, 4, 7), (0, 4, 7))
_MAJOR_SEVENTHS = ((0, 4, 7, 11), (0, 3, 7, 10), (0, 3, 7, 10),
                   (0, 4, 7, 11), (0, 4, 7, 10), (0, 3, 7, 10),
                   (0, 3, 6, 10))
_MINOR_SEVENTHS = ((0, 3, 7, 10), (0, 3, 6, 10), (0, 4, 7, 11),
                   (0, 3, 7, 10), (0, 3, 7, 10), (0, 4, 7, 11),
                   (0, 4, 7, 10))

# Mingus chord_shorthand keys verified from the pinned 0.6.1 source. "7" is
# deliberately handled as a key-derived seventh below, matching to_chords().
_SUFFIX_INTERVALS = {
    "M": (0, 4, 7), "m": (0, 3, 7), "5": (0, 7),
    "6": (0, 4, 7, 9), "M6": (0, 4, 7, 9), "m6": (0, 3, 7, 9),
    "69": (0, 2, 4, 7, 9), "9": (0, 2, 4, 7, 10),
    "M7": (0, 4, 7, 11), "m7": (0, 3, 7, 10),
    "m9": (0, 2, 3, 7, 10), "sus2": (0, 2, 7),
    "sus4": (0, 5, 7), "dim": (0, 3, 6),
    "dom7": (0, 4, 7, 10),
}


class RomanParseError(ValueError):
    """A source token could not be resolved without guessing."""

    def __init__(self, token, reason, code="invalid_token"):
        self.token = token
        self.reason = reason
        self.code = code
        super().__init__(f"{token!r}: {reason}")


def _quality_ids():
    registry = json.loads(QUALITY_REGISTRY_PATH.read_text(encoding="utf-8"))
    return {tuple(row["intervals"]): row["id"]
            for row in registry["qualities"]}


def _quality_id(intervals, token):
    result = _quality_ids().get(tuple(intervals))
    if result is None:
        raise RomanParseError(token,
                              f"resolver formula {list(intervals)} has no stable quality ID",
                              "unregistered_quality")
    return result


def _split_token(token):
    base, separator, bass = token.partition("/")
    if separator and (not bass or "/" in bass):
        raise RomanParseError(token, "slash bass must contain one Roman degree")
    return base, bass if separator else None


def _parse_degree(token):
    match = _TOKEN.fullmatch(token)
    if match is None:
        raise RomanParseError(token, "not a supported Roman chord token")
    degree_text = match.group("degree").upper()
    degree = ("I", "II", "III", "IV", "V", "VI", "VII").index(degree_text)
    accidental_text = match.group("acc")
    accidental = accidental_text.count("#") - accidental_text.count("b")
    suffix = match.group("suffix")
    return degree, accidental, suffix


def _context_data(context, source_dialect, token):
    if source_dialect not in ("major", "minor", "modal"):
        raise RomanParseError(token, f"unknown source dialect {source_dialect!r}",
                              "invalid_context")
    if not isinstance(context, dict):
        raise RomanParseError(token, "context must be a mapping", "invalid_context")
    expected = "minor" if source_dialect == "minor" else "major"
    tonality = context.get("tonality", expected)
    if tonality != expected:
        raise RomanParseError(token,
                              f"{source_dialect} source resolves against {expected} tonality",
                              "invalid_context")
    minor = tonality == "minor"
    return {
        "tonality": tonality,
        "root_offsets": _MINOR_ROOTS if minor else _MAJOR_ROOTS,
        "triads": _MINOR_TRIADS if minor else _MAJOR_TRIADS,
        "sevenths": _MINOR_SEVENTHS if minor else _MAJOR_SEVENTHS,
    }


def _resolve_one(token, context_data):
    base_token, bass_token = _split_token(token)
    degree, accidental, suffix = _parse_degree(base_token)
    root_offset = (context_data["root_offsets"][degree] + accidental) % 12

    if suffix == "":
        intervals = context_data["triads"][degree]
    elif suffix == "7":
        # Mingus explicitly treats I7 as the key-derived seventh, not dom7.
        intervals = context_data["sevenths"][degree]
    elif suffix in _SUFFIX_INTERVALS:
        intervals = _SUFFIX_INTERVALS[suffix]
    else:
        raise RomanParseError(token,
                              f"suffix {suffix!r} is absent from Mingus 0.6.1 chord_shorthand",
                              "unsupported_suffix")

    bass_offset = None
    if bass_token is not None:
        bass_degree, bass_accidental, bass_suffix = _parse_degree(bass_token)
        if bass_suffix:
            raise RomanParseError(token, "slash bass may specify only a Roman root")
        bass_offset = (context_data["root_offsets"][bass_degree] + bass_accidental) % 12

    return {
        "degree": degree + 1,
        "accidental": accidental,
        "root_offset": root_offset,
        "quality_id": _quality_id(intervals, token),
        "suffix": suffix,
        "slash_bass_offset": bass_offset,
        "slash_bass_interval": (bass_offset - root_offset) % 12 if bass_offset is not None else None,
        "source_spelling": base_token,
    }


def normalize_roman(text: str, context: dict, source_dialect: str) -> dict:
    """Return a canonical route while retaining source spellings and tokens.

    Source dialects reproduce the corpus generator's key choice: major/modal use
    a major key; minor uses a natural-minor key. Modal lower-case numerals do not
    silently override the resolver's key-derived quality.
    """
    if not isinstance(text, str):
        raise RomanParseError(str(text), "route must be text", "invalid_route")
    tokens = text.split()
    if not tokens:
        raise RomanParseError("", "route is empty", "empty_route")
    context_data = _context_data(context, source_dialect, tokens[0])
    route = [_resolve_one(token, context_data) for token in tokens]
    return {
        "source_dialect": source_dialect,
        "context": context_data["tonality"],
        "source_tokens": tokens,
        "route": route,
    }


def normalize_absolute_chords(text: str, tonic: str) -> dict:
    """Normalize a bounded absolute chord route relative to its tonic."""
    tonic_match = _ABSOLUTE_TOKEN.fullmatch(tonic)
    if tonic_match is None or tonic_match.group("suffix"):
        raise RomanParseError(tonic, "tonic must be an absolute note", "invalid_context")

    def note_pc(note):
        letter = note[0].upper()
        accidental = note[1:]
        if any(char not in "#b" for char in accidental) or ("#" in accidental and "b" in accidental):
            raise RomanParseError(note, "unsupported absolute note spelling")
        return (_PITCH_CLASS[letter] + accidental.count("#") - accidental.count("b")) % 12

    tokens = text.split()
    if not tokens:
        raise RomanParseError("", "route is empty", "empty_route")
    tonic_pc = note_pc(tonic_match.group("root"))
    route = []
    for token in tokens:
        match = _ABSOLUTE_TOKEN.fullmatch(token)
        if match is None:
            raise RomanParseError(token, "not a supported absolute chord token")
        root = note_pc(match.group("root"))
        suffix = match.group("suffix")
        intervals = _SUFFIX_INTERVALS.get(suffix, (0, 4, 7) if suffix == "" else None)
        if intervals is None:
            raise RomanParseError(token, f"unsupported absolute chord suffix {suffix!r}",
                                  "unsupported_suffix")
        route.append({
            "root_offset": (root - tonic_pc) % 12,
            "quality_id": _quality_id(intervals, token),
            "suffix": suffix,
            "slash_bass_offset": None,
            "slash_bass_interval": None,
            "source_spelling": token,
        })
    return {"tonic": tonic, "source_tokens": tokens, "route": route}
