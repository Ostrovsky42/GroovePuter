"""Discogs-400 style probabilities via Essentia (MTG-UPF).

Models are CC BY-NC-SA 4.0: fine for this offline research step; the model never
ships in firmware — only aggregated statistics do.

Files (put in --models dir; fetch with scripts/fetch_models.sh):
  discogs-effnet-bs64-1.pb                 embedding extractor
  genre_discogs400-discogs-effnet-1.pb     400-style classification head
  genre_discogs400-discogs-effnet-1.json   label list ("classes")
Node names follow the Essentia model docs; `gplab selfcheck` verifies they load.
"""
from __future__ import annotations

import json
from pathlib import Path

import numpy as np

EMB = "discogs-effnet-bs64-1.pb"
HEAD = "genre_discogs400-discogs-effnet-1.pb"
LABELS = "genre_discogs400-discogs-effnet-1.json"


def load_labels(models_dir) -> list[str]:
    meta = json.loads((Path(models_dir) / LABELS).read_text(encoding="utf-8"))
    return list(meta["classes"])


class StyleModel:
    def __init__(self, models_dir):
        from essentia.standard import (TensorflowPredict2D,  # lazy: requirements-ml
                                       TensorflowPredictEffnetDiscogs)
        d = Path(models_dir)
        self.labels = load_labels(d)
        self.emb = TensorflowPredictEffnetDiscogs(graphFilename=str(d / EMB), output="PartitionedCall:1")
        self.head = TensorflowPredict2D(graphFilename=str(d / HEAD),
                                        input="serving_default_model_Placeholder",
                                        output="PartitionedCall:0")

    def predict(self, audio_path, start: float = 0.0, duration: float = 60.0) -> dict[str, float]:
        from ..audio_io import load_mono
        audio = load_mono(audio_path, 16000, start=start, duration=duration)
        preds = np.asarray(self.head(self.emb(audio)))
        probs = preds.mean(axis=0)
        return {lab: float(p) for lab, p in zip(self.labels, probs)}
