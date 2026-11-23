"""Single bar schema shared by BOTH sides of the pipeline.

Corpus audio (after transcription) and GroovePuter takes (from gen_probe)
are written in the same format, so one feature extractor measures both.
See docs/CONTRACT.md for the normative description.
"""
from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable, Iterator

STEPS = 16
DRUM_ROLES = ("kick", "snare", "hat", "ohat")
ROLES = DRUM_ROLES + ("bass",)

# Event layouts (lists keep JSONL small):
#   drum role: [pos, vel]
#   bass:      [pos, vel, pitch, dur]
# pos  = float step position inside the bar, 0.0 <= pos < 16 (may be slightly
#        negative for early hits: they quantize to step 0)
# vel  = 0..1
# pitch= MIDI note number
# dur  = length in steps (float)


@dataclass
class Bar:
    src: str                      # "jamendo:1234" or "take:house.deep:3"
    variant: str                  # e.g. "house.deep"
    events: dict[str, list[list[float]]] = field(default_factory=dict)
    bar_idx: int = 0              # bar index inside source / phrase
    bpm: float | None = None
    take: int | None = None       # set for generated takes only

    def role(self, name: str) -> list[list[float]]:
        return self.events.get(name, [])

    def to_json(self) -> dict:
        d = {"src": self.src, "variant": self.variant, "bar_idx": self.bar_idx,
             "events": {k: v for k, v in self.events.items() if v}}
        if self.bpm is not None:
            d["bpm"] = round(float(self.bpm), 3)
        if self.take is not None:
            d["take"] = int(self.take)
        return d

    @classmethod
    def from_json(cls, d: dict) -> "Bar":
        unknown = set(d.get("events", {})) - set(ROLES)
        if unknown:
            raise ValueError(f"unknown roles {sorted(unknown)} in {d.get('src')}")
        return cls(src=d["src"], variant=d["variant"],
                   events={k: [list(map(float, e)) for e in v]
                           for k, v in d.get("events", {}).items()},
                   bar_idx=int(d.get("bar_idx", 0)), bpm=d.get("bpm"),
                   take=d.get("take"))


def read_bars(path: str | Path) -> Iterator[Bar]:
    path = Path(path)
    files = sorted(path.rglob("*.jsonl")) if path.is_dir() else [path]
    for f in files:
        with open(f, encoding="utf-8") as fh:
            for ln, line in enumerate(fh, 1):
                line = line.strip()
                if not line:
                    continue
                try:
                    yield Bar.from_json(json.loads(line))
                except (KeyError, ValueError, TypeError) as e:
                    raise ValueError(f"{f}:{ln}: bad bar record: {e}") from e


def write_bars(path: str | Path, bars: Iterable[Bar]) -> int:
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    n = 0
    with open(path, "w", encoding="utf-8") as fh:
        for b in bars:
            fh.write(json.dumps(b.to_json(), separators=(",", ":")) + "\n")
            n += 1
    return n
