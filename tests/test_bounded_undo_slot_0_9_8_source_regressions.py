#!/usr/bin/env python3
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/state/bounded_undo_slot.h"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def strip_cpp_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", "", text)


def main() -> None:
    text = HEADER.read_text(encoding="utf-8")
    code = strip_cpp_comments(text)

    require("template <std::size_t PayloadBytes>" in code,
            "R1 Undo storage must remain capacity-parametric until R2 measures payloads")
    require("class BoundedUndoSlot" in code,
            "bounded one-level Undo primitive is missing")
    require("std::array<uint8_t, PayloadBytes> payload_" in code,
            "Undo payload must remain fixed-capacity inline storage")
    require("SceneRevisionState revision_before_" in code,
            "Undo receipt must retain the exact pre-mutation revision state")
    require("std::is_trivially_copyable<Payload>::value" in code,
            "Undo payloads must be compile-time constrained to fixed values")
    require("kind == UndoKind::None || sizeof(Payload) > PayloadBytes" in code,
            "publish admission must reject invalid/oversized receipts")

    admission = code.index(
        "if (kind == UndoKind::None || sizeof(Payload) > PayloadBytes) return false;")
    write = code.index("std::memcpy(payload_.data(), &before, sizeof(Payload));")
    require(admission < write,
            "Undo admission must complete before retained history is overwritten")

    for token in (
        "std::vector",
        "std::deque",
        "malloc(",
        "calloc(",
        "realloc(",
        "operator new",
        "sceneTransactionScratch",
        "SceneManager",
        "AudioGuard",
        "Arduino.h",
        "SD.h",
    ):
        require(token not in code,
                f"R1 bounded Undo primitive crossed a forbidden ownership boundary: {token}")

    require("static BoundedUndoSlot" not in code and
            "inline BoundedUndoSlot" not in code,
            "R1 must not reserve a global fixed-DRAM Undo instance before R2 sizing")

    require("void clear()" in code and "kind_ = UndoKind::None" in code,
            "one-level Undo slot must expose explicit history consumption/reset")
    require("bool hasUndo() const" in code and "UndoKind kind() const" in code,
            "owner-facing state inspection contract is missing")

    print("0.9.8 R1 bounded Undo source regressions: PASS")


if __name__ == "__main__":
    main()
