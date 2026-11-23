"""Adapters to external ML tools. Every import is lazy so the core runs without them.

Status (verify before the first real run — APIs of research code drift):
  beats.py     beat_this (CPJKU, MIT)             — File2Beats API
  separate.py  demucs CLI (htdemucs)              — subprocess, stable
  drums.py     'bands' numpy fallback (tested) | 'midi' from any ADT tool (e.g. ADTOF)
  bass.py      basic-pitch note events | 'simple' numpy fallback (tested on synth)
  style.py     Essentia discogs-effnet + genre_discogs400 head (CC BY-NC-SA)
"""
