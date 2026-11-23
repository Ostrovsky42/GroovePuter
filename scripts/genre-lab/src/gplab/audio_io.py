"""Audio decode via stdlib wave (16-bit PCM WAV) or ffmpeg (anything else)."""
from __future__ import annotations

import shutil
import subprocess
import wave
from pathlib import Path

import numpy as np


def load_mono(path, sr: int, start: float = 0.0, duration: float | None = None) -> np.ndarray:
    path = Path(path)
    if path.suffix.lower() == ".wav":
        try:
            with wave.open(str(path)) as w:
                if w.getframerate() == sr and w.getsampwidth() == 2:
                    w.setpos(int(start * sr))
                    n = w.getnframes() - int(start * sr)
                    if duration is not None:
                        n = min(n, int(duration * sr))
                    raw = np.frombuffer(w.readframes(n), dtype=np.int16).astype(np.float32) / 32768
                    ch = w.getnchannels()
                    return raw.reshape(-1, ch).mean(1) if ch > 1 else raw
        except wave.Error:
            pass
    if not shutil.which("ffmpeg"):
        raise RuntimeError("ffmpeg not found; install it or provide 16-bit WAV at the target rate")
    cmd = ["ffmpeg", "-v", "error", "-ss", f"{start:.3f}"]
    if duration is not None:
        cmd += ["-t", f"{duration:.3f}"]
    cmd += ["-i", str(path), "-ac", "1", "-ar", str(sr), "-f", "f32le", "-"]
    raw = subprocess.run(cmd, check=True, capture_output=True).stdout
    return np.frombuffer(raw, dtype=np.float32).copy()


def probe_duration(path) -> float:
    path = Path(path)
    if path.suffix.lower() == ".wav":
        with wave.open(str(path)) as w:
            return w.getnframes() / w.getframerate()
    out = subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration",
                          "-of", "default=nw=1:nk=1", str(path)],
                         check=True, capture_output=True, text=True).stdout
    return float(out.strip())


def save_wav(path, y: np.ndarray, sr: int) -> None:
    y = np.clip(y, -1, 1)
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "w") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(sr)
        w.writeframes((y * 32767).astype(np.int16).tobytes())
