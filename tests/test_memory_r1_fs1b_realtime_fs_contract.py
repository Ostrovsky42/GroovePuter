#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
INO = ROOT / "GroovePuter.ino"
ENGINE = ROOT / "src" / "dsp" / "miniacid_engine.cpp"
RECORDER = ROOT / "src" / "audio" / "cardputer_audio_recorder.cpp"
SMF = ROOT / "src" / "platform" / "cardputer_smf_player.cpp"


def function_body(text: str, signature_pattern: str, owner: str) -> str:
    match = re.search(signature_pattern, text)
    if not match:
        raise AssertionError(f"{owner}: function signature not found: {signature_pattern}")
    start = text.find("{", match.end() - 1)
    if start < 0:
        raise AssertionError(f"{owner}: opening brace not found")
    depth = 0
    for index in range(start, len(text)):
        char = text[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return text[start + 1:index]
    raise AssertionError(f"{owner}: unterminated function body")


def reject(body: str, patterns: tuple[str, ...], owner: str) -> None:
    for pattern in patterns:
        if re.search(pattern, body):
            raise AssertionError(f"{owner}: hard-realtime filesystem/allocation token: {pattern}")


def require(body: str, pattern: str, owner: str) -> None:
    if not re.search(pattern, body):
        raise AssertionError(f"{owner}: expected contract token missing: {pattern}")


def main() -> None:
    ino = INO.read_text(encoding="utf-8")
    engine = ENGINE.read_text(encoding="utf-8")
    recorder = RECORDER.read_text(encoding="utf-8")
    smf = SMF.read_text(encoding="utf-8")

    audio_task = function_body(
        ino,
        r"\bvoid\s+audioTask\s*\([^)]*\)",
        "GroovePuter::audioTask",
    )
    render = function_body(
        engine,
        r"\bvoid\s+MiniAcid::generateAudioBuffer\s*\([^)]*\)",
        "MiniAcid::generateAudioBuffer",
    )

    forbidden = (
        r"\bSD\s*\.\s*open\s*\(",
        r"\bf_open\s*\(",
        r"\bf_close\s*\(",
        r"\.\s*close\s*\(",
        r"\bmalloc\s*\(",
        r"\bcalloc\s*\(",
        r"\brealloc\s*\(",
        r"\bfree\s*\(",
        r"\bnew\s+[^\n;(]",
        r"\bdelete\b",
        r"\bPatternPagingService::",
        r"\bSceneStorage",
        r"\bVoiceCache\b",
    )
    reject(audio_task, forbidden, "GroovePuter::audioTask")
    reject(render, forbidden, "MiniAcid::generateAudioBuffer")

    # Recording keeps its already-open File in the audio task. Allocation lives
    # in start()/stop(), while writeSamples() is I/O-only.
    require(audio_task, r"g_audioRecorder->writeSamples\s*\(", "audioTask recorder")
    recorder_write = function_body(
        recorder,
        r"\bvoid\s+CardputerAudioRecorder::writeSamples\s*\([^)]*\)",
        "CardputerAudioRecorder::writeSamples",
    )
    reject(
        recorder_write,
        (
            r"\bSD\s*\.\s*open\s*\(",
            r"\.\s*close\s*\(",
            r"\bf_open\s*\(",
            r"\bf_close\s*\(",
        ),
        "CardputerAudioRecorder::writeSamples",
    )

    # SMF command consumption is owned by the dedicated task. taskLoop() may
    # delegate command semantics, but Load -> loadFile -> source_.open must stay
    # under that chain rather than migrating into AudioTask.
    task_loop = function_body(
        smf,
        r"\bvoid\s+CardputerSmfPlayerService::taskLoop\s*\([^)]*\)",
        "CardputerSmfPlayerService::taskLoop",
    )
    handle_command = function_body(
        smf,
        r"\bvoid\s+CardputerSmfPlayerService::handleCommand\s*\([^)]*\)",
        "CardputerSmfPlayerService::handleCommand",
    )
    load_file = function_body(
        smf,
        r"\bbool\s+CardputerSmfPlayerService::loadFile\s*\([^)]*\)",
        "CardputerSmfPlayerService::loadFile",
    )
    require(task_loop, r"handleCommand\s*\(", "SMF taskLoop")
    require(handle_command, r"CommandType::Load", "SMF handleCommand")
    require(handle_command, r"loadFile\s*\(", "SMF handleCommand")
    require(load_file, r"source_\.open\s*\(", "SMF loadFile")

    print("MEMORY-R1 FS1B realtime filesystem contract: PASS")


if __name__ == "__main__":
    main()
