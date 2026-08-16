from pathlib import Path

def replace_once(path, old, new):
    p = Path(path)
    s = p.read_text()
    if s.count(old) != 1:
        raise SystemExit(f"{path}: anchor count={s.count(old)}")
    p.write_text(s.replace(old, new, 1))

replace_once(
    "src/input/midi_input_router.h",
    "    uint32_t unmappedDrumNotes{0};\n};",
    "    uint32_t unmappedDrumNotes{0};\n"
    "    uint32_t sessionCleanups{0};\n"
    "    uint32_t sessionNotesReleased{0};\n"
    "};")

replace_once(
    "src/input/midi_input_router.h",
    "    void panic() { releaseAllOwnedNotes(); }\n\n"
    "    std::size_t activeNoteCount() const {",
    "    void panic() { releaseAllOwnedNotes(); }\n\n"
    "    std::size_t releaseSession(MidiInputTransportId transportId,\n"
    "                               MidiInputSessionId sessionId) {\n"
    "        if (transportId == kInvalidMidiInputTransportId ||\n"
    "            sessionId == kInvalidMidiInputSessionId) {\n"
    "            return 0u;\n"
    "        }\n"
    "        std::size_t released = 0;\n"
    "        for (std::size_t i = 0; i < kMaxActiveNotes; ++i) {\n"
    "            const auto& owner = owners_[i];\n"
    "            if (!owner.active || owner.transportId != transportId ||\n"
    "                owner.sessionId != sessionId) {\n"
    "                continue;\n"
    "            }\n"
    "            releaseOwner(i);\n"
    "            ++released;\n"
    "        }\n"
    "        ++diagnostics_.sessionCleanups;\n"
    "        diagnostics_.sessionNotesReleased += static_cast<uint32_t>(released);\n"
    "        return released;\n"
    "    }\n\n"
    "    std::size_t activeNoteCount() const {")

replace_once(
    "src/platform/cardputer_usb_midi_transport.cpp",
    "void resetMidiInputSession() {\n"
    "    if (g_midiInputQueue != nullptr) {\n"
    "        g_midiInputQueue->discardPendingFromConsumer();\n"
    "    }\n"
    "    if (g_midiInputRouter != nullptr) {\n"
    "        g_midiInputRouter->panic();\n"
    "    }\n"
    "}\n",
    "void resetMidiInputSession() {\n"
    "    if (g_midiInputQueue != nullptr) {\n"
    "        g_midiInputQueue->discardPendingFromConsumer();\n"
    "    }\n"
    "    if (g_midiInputRouter != nullptr &&\n"
    "        g_midiInputSession != kInvalidMidiInputSessionId) {\n"
    "        (void)g_midiInputRouter->releaseSession(\n"
    "            kCardputerUsbInputTransportId,\n"
    "            g_midiInputSession);\n"
    "    }\n"
    "}\n")

Path("tools/r5_bootstrap.py").unlink()
