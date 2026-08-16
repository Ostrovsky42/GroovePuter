#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / "src/ui/pages/project_page.h").read_text()
CPP = (ROOT / "src/ui/pages/project_page.cpp").read_text()
HELPER = (ROOT / "src/ui/midi_device_profile_ui.h").read_text()


def require(text: str, needle: str, where: str) -> None:
    if needle not in text:
        raise AssertionError(f"missing {needle!r} in {where}")


def forbid(text: str, needle: str, where: str) -> None:
    if needle in text:
        raise AssertionError(f"forbidden {needle!r} in {where}")


require(HEADER, "enum class ProjectSection { Scenes = 0, Groove, Led, Midi };", "project_page.h")
require(HEADER, "LedFlash, MidiDevice", "project_page.h")
require(HEADER, "uint8_t midi_profile_preview_ = 0xFF;", "project_page.h")
require(HEADER, "static_assert(sizeof(ProjectPage) <= 256", "project_page.h")
forbid(HEADER, "MidiOutputSettings", "project_page.h")
forbid(HEADER, "MidiOutputRouteProjection", "project_page.h")

require(CPP, "case 3: return \"MIDI\";", "project_page.cpp")
require(CPP, "case 3: // midi", "project_page.cpp")
require(CPP, "sectionIdx = (sectionIdx + 1) % 4;", "project_page.cpp")
require(CPP, "MainFocus::MidiDevice", "project_page.cpp")
require(CPP, "ProfileUi::stepSelectableProfile", "project_page.cpp")
require(CPP, "selectCardputerMidiDeviceProfileForNextBoot", "project_page.cpp")
require(CPP, "cardputerMidiDeviceProfileRestartRequired", "project_page.cpp")
require(CPP, "Apply:REBOOT", "project_page.cpp")
require(CPP, "Apply:ENTER SAVE", "project_page.cpp")
forbid(CPP, "publishMidiPatternStartupRoutes", "project_page.cpp")
forbid(CPP, "applyMidiDeviceProfile(", "project_page.cpp")

require(HELPER, "MidiDeviceProfile::SeqtrakNative", "midi_device_profile_ui.h")
require(HELPER, "MidiDeviceProfile::GeneralMidi", "midi_device_profile_ui.h")
require(HELPER, "MidiDeviceProfile::GenericMidi", "midi_device_profile_ui.h")
require(HELPER, "MidiDeviceProfile::Custom", "midi_device_profile_ui.h")
require(HELPER, "CUSTOM is display-only", "midi_device_profile_ui.h")

print("0.9.7-R8 Device Profile UI source regressions: PASS")
