#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_once(relative: str, old: str, new: str) -> None:
    path = ROOT / relative
    text = path.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"expected one guarded block in {relative}, found {count}: {old[:80]!r}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


# UI screen index and pattern-storage page index are independent domains.
replace_once(
    "src/ui/miniacid_display.cpp",
    """    pages_[page_index_] = createPage_(page_index_);
    mini_acid_.setCurrentPage(static_cast<int8_t>(page_index_));
""",
    """    pages_[page_index_] = createPage_(page_index_);
""",
)
replace_once(
    "src/ui/miniacid_display.cpp",
    """    previous_page_index_ = page_index_;
    page_index_ = index;
    mini_acid_.setCurrentPage(static_cast<int8_t>(page_index_));

    IPage* newPage = getPage_(index);
""",
    """    previous_page_index_ = page_index_;
    page_index_ = index;

    IPage* newPage = getPage_(index);
""",
)

# Exact input priority: modal/hard globals -> page command -> live note -> legacy fallback.
replace_once(
    "src/ui/miniacid_display.cpp",
    """    }

    // 2) Global navigation fallback
    if (event.event_type == GROOVEPUTER_KEY_DOWN) {
""",
    """    }

    // 2) Centralized performance input. The active page gets first refusal;
    // only an unhandled plain key can become a live note.
    if (event.event_type == GROOVEPUTER_KEY_DOWN &&
        !event.alt && !event.ctrl && !event.shift && !event.meta &&
        WorkflowPages::allowsPerformanceKeyboard(page_index_) &&
        performance_keyboard_.keyDown(event.key)) {
        return true;
    }

    // 3) Global navigation fallback
    if (event.event_type == GROOVEPUTER_KEY_DOWN) {
""",
)
replace_once(
    "GroovePuter.ino",
    """    // Page commands have priority. Only unhandled plain keys become notes.
    if (!evt.alt && !evt.ctrl && !evt.shift && !evt.meta &&
        g_miniDisplay &&
        WorkflowPages::allowsPerformanceKeyboard(g_miniDisplay->currentPageIndex()) &&
        g_performanceKeyboard.keyDown(evt.key)) {
      drawUI();
      return;
    }

""",
    "",
)

# Desktop release path mirrors the hardware matrix reconciliation contract.
replace_once(
    "platform_sdl/sdl_main.cpp",
    """      } else if (sc == SDL_SCANCODE_L) {
        SDL_LockAudioDevice(s.audio.device);
        s.audio.synth.setBpm(s.audio.synth.bpm() + 5.0f);
        SDL_UnlockAudioDevice(s.audio.device);
      }
    }
""",
    """      } else if (sc == SDL_SCANCODE_L) {
        SDL_LockAudioDevice(s.audio.device);
        s.audio.synth.setBpm(s.audio.synth.bpm() + 5.0f);
        SDL_UnlockAudioDevice(s.audio.device);
      }
    } else if (e.type == SDL_KEYUP) {
      const SDL_Keycode keycode = e.key.keysym.sym;
      const bool modified = (e.key.keysym.mod & (KMOD_ALT | KMOD_CTRL | KMOD_SHIFT | KMOD_GUI)) != 0;
      if (!modified && keycode >= 32 && keycode < 127) {
        s.keyboard.keyUp(static_cast<char>(keycode));
      }
    }
""",
)

# Update the permanent source contract to protect the corrected routing boundary.
path = ROOT / "tests/test_source_regressions.py"
text = path.read_text(encoding="utf-8")
text = text.replace(
    """    require("g_performanceKeyboard.keyDown" in sketch,
            "hardware input must route unhandled keys through PerformanceKeyboard")
""",
    """    require("performance_keyboard_.keyDown" in display,
            "display input policy must route unhandled page keys through PerformanceKeyboard")
    require("g_performanceKeyboard.keyDown" not in sketch,
            "hardware sketch must not duplicate normalized note routing")
""",
    1,
)
text = text.replace(
    """    require("case 12: page = std::make_unique<PerformPage>" in display,
            "PERFORM page must remain an additive page instead of reindexing editors")
""",
    """    require("case 12: page = std::make_unique<PerformPage>" in display,
            "PERFORM page must remain an additive page instead of reindexing editors")
    require("setCurrentPage(static_cast<int8_t>(page_index_))" not in display,
            "UI page indices must never overwrite pattern-storage page indices")
""",
    1,
)
path.write_text(text, encoding="utf-8")

print("Performance input ordering fix complete")
