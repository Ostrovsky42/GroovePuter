#!/usr/bin/env python3
"""SOURCE and MAKE PHRASE must both be reachable and keep separate semantics."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PARAMS = (ROOT / "src/ui/pages/tb303_params_page.cpp").read_text(encoding="utf-8")
SYNTH = (ROOT / "src/ui/pages/synth_sequencer_page.cpp").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    require("PhraseSourceToggle::toggle" in PARAMS,
            "the SRC row must go through the shared owner")
    require("PhraseSourceToggle::toggle" in SYNTH,
            "the hotkey must go through the shared owner")
    require('source_control_->setLabel("SRC")' in PARAMS,
            "MORE must expose a dedicated SRC row")
    require('make_phrase_control_->setLabel("MAKE PHRASE")' in PARAMS,
            "MORE must expose a dedicated MAKE PHRASE row")
    require("PhraseSourceToggle::makePhrase" in PARAMS,
            "the MAKE PHRASE row must call the shared materialization owner")

    # The discriminator is the conversion decision, not the receipt. A first
    # draft of this gate forbade "UndoKind::RuntimePhrase, receipt" outright
    # and caught commitRuntimePhraseEditWithUndo, which records a *material*
    # edit -- a different owner that legitimately builds its own receipt.
    # What must not exist twice is the choice between converting, switching and
    # returning, and makePhrase is the only call unique to it.
    for name, text in (("tb303_params_page", PARAMS),
                       ("synth_sequencer_page", SYNTH)):
        require("mini_acid_.makePhrase(" not in text,
                f"{name} bypasses the shared materialization owner")

    require("ALT+R" in SYNTH,
            "the switch must be advertised on the screen it governs")


if __name__ == "__main__":
    main()
