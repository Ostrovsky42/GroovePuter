from pathlib import Path
transport = Path("src/platform/cardputer_usb_midi_transport.cpp").read_text()
service = Path("src/platform/cardputer_usb_midi_service.h").read_text()
app = Path("GroovePuter.ino").read_text()
required = [
    '#include "src/input/midi_input_queue.h"',
    '#include "src/input/midi_input_router.h"',
    '#include "src/midi/usb_midi_channel_voice_parser.h"',
    'serviceMidiInputConnection();',
    'g_midiInputRouter->service(*g_midiInputQueue, kMidiRxDrainBudget);',
    'parseUsbMidiChannelVoice(',
    'parseUsbMidiRealtimeTransport(',
    'kCardputerUsbInputTransportId',
    'nextMidiInputSession()',
    'g_midiInputQueue->discardPendingFromConsumer();',
]
for token in required:
    assert token in transport, token

# R3b2 originally used a conservative input-wide panic on a USB lifetime edge.
# R5 narrows that cleanup to the retired transport/session while preserving all
# other R3b2 runtime ownership guarantees.
if Path("docs/releases/0_9_10_R5_LIFECYCLE.md").exists():
    assert 'g_midiInputRouter->releaseSession(' in transport
else:
    assert 'g_midiInputRouter->panic();' in transport

assert "tud_midi_rx_cb" not in transport
assert transport.count("tud_midi_packet_read(") == 1
assert transport.count("xTaskCreateStaticPinnedToCore(") == 1
assert "MidiInputQueue& midiInputQueue" in service
assert "MidiInputRouter& midiInputRouter" in service
assert "static MidiInputQueue g_midiInputQueue;" in app
assert "static MidiInputRouter g_midiInputRouter(g_musicalEventRouter);" in app
rx = transport.index("void drainIncomingMidiPackets()")
assert transport.index("parseUsbMidiRealtimeTransport(", rx) < transport.index("parseUsbMidiChannelVoice(", rx)
print("0.9.10 R3b2 runtime ownership/source boundaries: PASS")
