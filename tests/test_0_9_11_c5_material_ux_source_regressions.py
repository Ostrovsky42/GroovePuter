#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    workflow = (ROOT / "src/ui/workflow_mode.h").read_text(encoding="utf-8")
    phrase = (ROOT / "src/ui/pages/phrase_page.cpp").read_text(encoding="utf-8")
    genre = (ROOT / "src/ui/pages/genre_page.cpp").read_text(encoding="utf-8")
    synth = (ROOT / "src/ui/pages/synth_sequencer_page.cpp").read_text(encoding="utf-8")
    generation = (ROOT / "src/state/generation_request_state.h").read_text(encoding="utf-8")

    # Public navigation names the musical object, not the retained runtime seam.
    require('case kPhrase: return "MATERIAL";' in workflow,
            "Song workflow must expose MATERIAL instead of PHRASE")
    require('case kPhraseCore: return "MATERIAL BANK";' in workflow,
            "legacy PhraseCore tools must be subordinate to MATERIAL")

    # P1/P2/P3 remain internal realization levels, but the musician sees STYLE.
    require('generationStyleName' in generation,
            "generation realization needs a musician-facing STYLE projection")
    for name in ("FAITHFUL", "VARIANT", "REWORK"):
        require(f'return "{name}"' in generation,
                f"missing musician-facing style name: {name}")
    require('drawStandardHeader(gfx, mini_acid_, "MATERIAL")' in phrase,
            "product page header must identify MATERIAL")
    require('"STYLE"' in phrase and 'generationStyleName(depth)' in phrase,
            "Material request must expose STYLE instead of P-level engineering vocabulary")
    require('"LAST TRY: %s"' in phrase,
            "generation outcome must be phrased as a user action, not LAST G")
    require('"G CREATES A NEW TAKE"' in phrase,
            "empty Material state must teach the musical action")

    # Genre application is a musical decision about the current/new take.
    require('return "STYLE ONLY";' in genre,
            "genre profile-only mode must be named STYLE ONLY")
    require('return "NEW TAKE";' in genre,
            "genre regeneration must be named NEW TAKE")
    require('return "NEW TAKE + TEMPO";' in genre,
            "genre regeneration+tempo must be named NEW TAKE + TEMPO")

    # Local editor may reveal the sounding representation, but not Pattern/Phrase
    # as competing user entities and not a MAKE PHRASE conversion command.
    require('"MATERIAL"' in synth and '"PLAY:MELODY"' in synth,
            "melody editor must stay inside the MATERIAL model while telling source truth")
    require('"SOURCE: STEPS"' in synth and '"SOURCE: MELODY"' in synth,
            "source truth must use representation vocabulary")
    require('"MAKE PHRASE"' not in synth,
            "MAKE PHRASE must not remain a primary user-facing seam")

    print("0.9.11 C5 Material UX source contracts: OK")


if __name__ == "__main__":
    main()
