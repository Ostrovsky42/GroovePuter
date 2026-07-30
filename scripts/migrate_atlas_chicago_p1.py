#!/usr/bin/env python3
"""Integrate the compiled Chicago Jack P1 Atlas vertical slice."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one match, found {count}: {old[:120]}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


def patch_genre_manager() -> None:
    path = ROOT / "src/dsp/genre_manager.cpp"

    replace_once(
        path,
        """    {5, "Dub Techno",
     {3, 6, 0.12f, 0.16f, 72, 108, 0.78f, 0.28f, 0.05f, 0.18f, 1, 1, 1, 0.30f, 1, 6},
     true,
     {0x8080, 0x0808, 0x2222, 0x0202, 0.10f, 0.06f, 0.22f, 92, 88, 70, true, false}},
};
""",
        """    {5, "Dub Techno",
     {3, 6, 0.12f, 0.16f, 72, 108, 0.78f, 0.28f, 0.05f, 0.18f, 1, 1, 1, 0.30f, 1, 6},
     true,
     {0x8080, 0x0808, 0x2222, 0x0202, 0.10f, 0.06f, 0.22f, 92, 88, 70, true, false}},
    // Atlas v2.6 vertical slice. Exact P1/P2/P3 events are compiled into
    // AtlasRuntime; this fallback remains useful for manual randomize actions.
    {6, "Chicago Jack",
     {8, 13, 0.02f, 0.04f, 76, 122, 0.58f, 0.04f, 0.08f, 0.35f, 0, 0, 0, 0.20f, 0, 8},
     true,
     {0x8888, 0x0808, 0x2222, 0x0202, 0.04f, 0.04f, 0.18f, 122, 102, 82, false, true}},
};
""",
    )

    replace_once(
        path,
        """        case 5: return GrooveboxMode::Dub;    // Dub Techno
        case 0: break;                        // base layer: use fallback
""",
        """        case 5: return GrooveboxMode::Dub;    // Dub Techno
        case 6: return GrooveboxMode::Acid;   // Atlas: Chicago Jack
        case 0: break;                        // base layer: use fallback
""",
    )

    replace_once(
        path,
        """    GenreBehavior b = kBase[static_cast<int>(state_.generative)];

    return b;
""",
        """    GenreBehavior b = kBase[static_cast<int>(state_.generative)];

    if (state_.recipe == 6) {
        // GroovePuter preview sound profile. Atlas supplies musical events but
        // intentionally does not claim verified SEQTRAK preset mappings.
        b.stepMask = 0xFFFF;
        b.motifLength = 4;
        b.preferredScale = 1;  // Phrygian
        b.useMotif = true;
        b.allowChromatic = true;
        b.forceOctaveJump = false;
        b.avoidClusters = false;
        b.timbre = {0.0f, 0.52f, 0.55f, 0.82f, 0.25f};
    }

    return b;
""",
    )


def patch_engine() -> None:
    path = ROOT / "src/dsp/miniacid_engine.cpp"

    replace_once(
        path,
        """#include "swappable_synth_voice.h"
#include "advanced_pattern_generator.h"
""",
        """#include "swappable_synth_voice.h"
#include "advanced_pattern_generator.h"
#include "atlas_runtime.h"
""",
    )

    replace_once(
        path,
        """  syncGrooveModeToGenre();
  
  const GenerativeParams& genreParams =
      genreManager_.getCompiledGenerativeParams();
""",
        """  syncGrooveModeToGenre();

  AtlasRuntimeMetadata atlasMetadata{};
  if (AtlasRuntime::applyRecipe(
          genreManager_.recipe(), 0,
          editSynthPattern(0), editSynthPattern(1),
          sceneManager_.editCurrentDrumPattern(), &atlasMetadata)) {
    Scene& scene = sceneManager_.currentScene();
    scene.feel.swingPct = atlasMetadata.swingPercent;
    if (scene.genre.applyTempoOnApply) {
      setBpm(static_cast<float>(atlasMetadata.bpm));
    }
    LOG_DEBUG("  - Atlas recipe applied: %s %s bpm=%u swing=%u\\n",
              atlasMetadata.displayName, atlasMetadata.slotId,
              static_cast<unsigned>(atlasMetadata.bpm),
              static_cast<unsigned>(atlasMetadata.swingPercent));
    return;
  }

  const GenerativeParams& genreParams =
      genreManager_.getCompiledGenerativeParams();
""",
    )


def main() -> None:
    patch_genre_manager()
    patch_engine()


if __name__ == "__main__":
    main()
