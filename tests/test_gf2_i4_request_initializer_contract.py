#!/usr/bin/env python3
"""GF2-I4 source contract for request aggregate initialization.

I4 appended an optional structural-density carrier to three request structs.
Every existing call site intentionally uses an empty/default initializer followed
by member assignment. A non-empty positional aggregate initializer would make a
future field insertion capable of silently rebinding semantic values, so this
regression rejects that source shape across production, tests, and tools.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REQUEST_TYPES = (
    "RhythmRealizationRequest",
    "BarEvolutionRequest",
    "PhraseEvolutionRequest",
)
CODE_SUFFIXES = {".cpp", ".cc", ".cxx", ".h", ".hpp", ".ino"}
SKIP_DIRS = {".git", "build", ".pio", "node_modules"}

COMMENT_OR_LITERAL = re.compile(
    r"//[^\n]*|/\*.*?\*/|\"(?:\\.|[^\"\\])*\"|'(?:\\.|[^'\\])*'",
    re.DOTALL,
)
TYPE_TOKEN = re.compile(r"\b(" + "|".join(REQUEST_TYPES) + r")\b")
IDENTIFIER = re.compile(r"[A-Za-z_]\w*")


def sanitize(source: str) -> str:
    def replace(match: re.Match[str]) -> str:
        text = match.group(0)
        return "".join("\n" if ch == "\n" else " " for ch in text)

    return COMMENT_OR_LITERAL.sub(replace, source)


def skip_space(source: str, offset: int) -> int:
    while offset < len(source) and source[offset].isspace():
        offset += 1
    return offset


def line_for(source: str, offset: int) -> int:
    return source.count("\n", 0, offset) + 1


def preceding_word(source: str, offset: int) -> str:
    prefix = source[:offset].rstrip()
    match = re.search(r"([A-Za-z_]\w*)$", prefix)
    return match.group(1) if match else ""


def brace_is_empty(source: str, brace: int) -> bool:
    after = skip_space(source, brace + 1)
    return after < len(source) and source[after] == "}"


def classify_file(path: Path) -> tuple[list[str], list[str]]:
    original = path.read_text(encoding="utf-8")
    source = sanitize(original)
    safe: list[str] = []
    unsafe: list[str] = []

    for match in TYPE_TOKEN.finditer(source):
        type_name = match.group(1)
        cursor = skip_space(source, match.end())

        # Anonymous direct-list expression: Type{...}. Ignore the actual
        # struct/class definition, but reject non-empty positional temporaries.
        if cursor < len(source) and source[cursor] == "{":
            if preceding_word(source, match.start()) in {"struct", "class"}:
                continue
            line = line_for(source, match.start())
            label = f"{path.relative_to(ROOT)}:{line} {type_name} anonymous list"
            if brace_is_empty(source, cursor):
                safe.append(label)
            else:
                unsafe.append(label)
            continue

        identifier = IDENTIFIER.match(source, cursor)
        if identifier is None:
            continue
        cursor = skip_space(source, identifier.end())

        # Skip optional array declarators before a possible copy-list init.
        while cursor < len(source) and source[cursor] == "[":
            closing = source.find("]", cursor + 1)
            if closing < 0:
                break
            cursor = skip_space(source, closing + 1)

        brace = -1
        if cursor < len(source) and source[cursor] == "{":
            brace = cursor
        elif cursor < len(source) and source[cursor] == "=":
            cursor = skip_space(source, cursor + 1)
            if cursor < len(source) and source[cursor] == "{":
                brace = cursor

        if brace < 0:
            continue

        line = line_for(source, match.start())
        variable = identifier.group(0)
        label = (
            f"{path.relative_to(ROOT)}:{line} {type_name} {variable}"
        )
        if brace_is_empty(source, brace):
            safe.append(label)
        else:
            unsafe.append(label)

    return safe, unsafe


def code_files() -> list[Path]:
    files: list[Path] = []
    for path in ROOT.rglob("*"):
        if not path.is_file() or path.suffix not in CODE_SUFFIXES:
            continue
        relative = path.relative_to(ROOT)
        if any(part in SKIP_DIRS for part in relative.parts):
            continue
        files.append(path)
    return sorted(files)


def require_default_sentinel(path: str, type_name: str) -> None:
    source = (ROOT / path).read_text(encoding="utf-8")
    start = source.find(f"struct {type_name}")
    if start < 0:
        raise AssertionError(f"{type_name}: struct definition missing")
    end = source.find("};", start)
    if end < 0:
        raise AssertionError(f"{type_name}: struct terminator missing")
    body = source[start:end]
    expected = "uint8_t structuralDensityTarget = kNoStructuralDensityTarget;"
    if expected not in body:
        raise AssertionError(
            f"{type_name}: optional density field must retain explicit sentinel default"
        )


def main() -> int:
    require_default_sentinel(
        "src/generation/rhythm/rhythm_realizer.h", "RhythmRealizationRequest"
    )
    require_default_sentinel(
        "src/generation/rhythm/bar_evolution.h", "BarEvolutionRequest"
    )
    require_default_sentinel(
        "src/generation/phrase/phrase_evolution.h", "PhraseEvolutionRequest"
    )

    safe: list[str] = []
    unsafe: list[str] = []
    for path in code_files():
        file_safe, file_unsafe = classify_file(path)
        safe.extend(file_safe)
        unsafe.extend(file_unsafe)

    for item in safe:
        print(f"SAFE   {item} — empty/default initializer; optional field uses sentinel")
    for item in unsafe:
        print(f"UNSAFE {item} — non-empty positional aggregate initializer", file=sys.stderr)

    if unsafe:
        print(
            f"GF2-I4 aggregate initializer compatibility: FAIL ({len(unsafe)} unsafe)",
            file=sys.stderr,
        )
        return 1

    if not safe:
        print(
            "GF2-I4 aggregate initializer compatibility: FAIL (no request initializers found)",
            file=sys.stderr,
        )
        return 1

    print(
        "GF2-I4 aggregate initializer compatibility: "
        f"PASS ({len(safe)} SAFE, 0 UNSAFE)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
