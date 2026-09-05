from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected exactly one match, found {count}")
    return text.replace(old, new, 1)


header_path = Path("src/midi/usb_midi_output.h")
header = header_path.read_text()
header = replace_once(
    header,
    "    bool releaseAbandonedSmfChannels();\n",
    "    void requestChannelPanic(uint8_t channel);\n"
    "    bool releasePendingChannelPanics();\n",
    "header helper declaration",
)
header = replace_once(
    header,
    "    uint16_t abandonedSmfChannels_;\n",
    "    uint16_t pendingChannelPanics_;\n",
    "header pending mask",
)
header_path.write_text(header)

source_path = Path("src/midi/usb_midi_output.cpp")
source = source_path.read_text()

old_member_count = source.count("abandonedSmfChannels_")
old_helper_count = source.count("releaseAbandonedSmfChannels")
if old_member_count != 6:
    raise SystemExit(
        f"pending mask rename: expected 6 matches, found {old_member_count}"
    )
if old_helper_count != 3:
    raise SystemExit(
        f"pending helper rename: expected 3 matches, found {old_helper_count}"
    )
source = source.replace("abandonedSmfChannels_", "pendingChannelPanics_")
source = source.replace("releaseAbandonedSmfChannels", "releasePendingChannelPanics")

old_target_cleanup = '''void UsbMidiOutput::releaseTargetAllNotes(MusicalEventSource source,
                                          MusicalEventTarget target) {
    if (target != MusicalEventTarget::Drums &&
        isSynthPerformanceSource(source)) {
        releaseGeneratedTarget(target);
        return;
    }

    for (std::size_t i = 0; i < kLaneCount; ++i) {
        if (lanes_[i].source != source || lanes_[i].target != target) continue;
        if (target == MusicalEventTarget::Drums) {
            releasePercussiveLane(lanes_[i]);
        } else {
            releaseActiveNote(lanes_[i]);
        }
    }
}
'''
new_target_cleanup = '''void UsbMidiOutput::requestChannelPanic(uint8_t channel) {
    pendingChannelPanics_ |=
        static_cast<uint16_t>(1u << clampChannel(channel));
}

void UsbMidiOutput::releaseTargetAllNotes(MusicalEventSource source,
                                          MusicalEventTarget target) {
    if (target != MusicalEventTarget::Drums &&
        isSynthPerformanceSource(source)) {
        requestChannelPanic(generatedChannel(target));
        releaseGeneratedTarget(target);
        releasePendingChannelPanics();
        return;
    }

    for (std::size_t i = 0; i < kLaneCount; ++i) {
        if (lanes_[i].source != source || lanes_[i].target != target) continue;
        requestChannelPanic(lanes_[i].channel);
        if (target == MusicalEventTarget::Drums) {
            releasePercussiveLane(lanes_[i]);
        } else {
            releaseActiveNote(lanes_[i]);
        }
    }
    releasePendingChannelPanics();
}
'''
source = replace_once(
    source,
    old_target_cleanup,
    new_target_cleanup,
    "scoped target cleanup",
)

source = replace_once(
    source,
    '''    releaseGeneratedTarget(MusicalEventTarget::SynthA);
    releaseGeneratedTarget(MusicalEventTarget::SynthB);
    releaseGeneratedTarget(MusicalEventTarget::Dx);
}

bool UsbMidiOutput::handleSmfNoteOn''',
    '''    releaseGeneratedTarget(MusicalEventTarget::SynthA);
    releaseGeneratedTarget(MusicalEventTarget::SynthB);
    releaseGeneratedTarget(MusicalEventTarget::Dx);
    releasePendingChannelPanics();
}

bool UsbMidiOutput::handleSmfNoteOn''',
    "all active cleanup drain",
)

source = replace_once(
    source,
    '''    pollConnection();
    if (!enabled_ || !begun_ || !mounted_) return false;

    const uint8_t channel = clampChannel(zeroBasedChannel);''',
    '''    pollConnection();
    if (!enabled_ || !begun_ || !mounted_) return false;
    releasePendingChannelPanics();

    const uint8_t channel = clampChannel(zeroBasedChannel);''',
    "SMF NoteOn pre-drain",
)

old_smf_off = '''bool UsbMidiOutput::handleSmfNoteOff(uint8_t zeroBasedChannel,
                                     uint8_t note,
                                     uint8_t velocity) {
    pollConnection();
    const uint8_t channel = clampChannel(zeroBasedChannel);
    note = clampDataByte(note);
    velocity = clampDataByte(velocity);

    auto* cell = owners_.peek(channel, note);
    if (cell == nullptr || cell->smf == 0) return true;

    if (cell->wire > 1) {
        --cell->smf;
        --cell->wire;
        owners_.prune();
        return true;
    }

    if (cell->wire == 0) {
        cell->smf = 0;
        owners_.prune();
        return true;
    }

    if (!mounted_ || !transport_.sendNoteOff(channel, note, velocity)) {
        return false;
    }
    transport_.flush();
    cell->smf = 0;
    cell->wire = 0;
    owners_.prune();
    return true;
}
'''
new_smf_off = '''bool UsbMidiOutput::handleSmfNoteOff(uint8_t zeroBasedChannel,
                                     uint8_t note,
                                     uint8_t velocity) {
    pollConnection();
    const uint8_t channel = clampChannel(zeroBasedChannel);
    note = clampDataByte(note);
    velocity = clampDataByte(velocity);

    auto* cell = owners_.peek(channel, note);
    if (cell == nullptr || cell->smf == 0) {
        releasePendingChannelPanics();
        return true;
    }

    if (cell->wire > 1) {
        --cell->smf;
        --cell->wire;
        owners_.prune();
        releasePendingChannelPanics();
        return true;
    }

    if (cell->wire == 0) {
        cell->smf = 0;
        owners_.prune();
        releasePendingChannelPanics();
        return true;
    }

    if (!mounted_ || !transport_.sendNoteOff(channel, note, velocity)) {
        return false;
    }
    transport_.flush();
    cell->smf = 0;
    cell->wire = 0;
    owners_.prune();
    releasePendingChannelPanics();
    return true;
}
'''
source = replace_once(source, old_smf_off, new_smf_off, "SMF NoteOff post-drain")

source = replace_once(
    source,
    '''void UsbMidiOutput::handleMusicalEvent(const MusicalEvent& event) {
    if (!begun_) return;
    pollConnection();

    if (event.type == MusicalEventType::AllNotesOff) {''',
    '''void UsbMidiOutput::handleMusicalEvent(const MusicalEvent& event) {
    if (!begun_) return;
    pollConnection();
    releasePendingChannelPanics();

    if (event.type == MusicalEventType::AllNotesOff) {''',
    "musical event pre-drain",
)

source = replace_once(
    source,
    '''        if (!retryGeneratedPendingReleases(event.target)) return;
        switch (event.type) {''',
    '''        if (!retryGeneratedPendingReleases(event.target)) return;
        releasePendingChannelPanics();
        switch (event.type) {''',
    "generated pre-dispatch drain",
)

source = replace_once(
    source,
    '''            case MusicalEventType::AllNotesOff:
                break;
        }
        return;
    }

    MidiVoiceLane* lane = laneFor''',
    '''            case MusicalEventType::AllNotesOff:
                break;
        }
        releasePendingChannelPanics();
        return;
    }

    MidiVoiceLane* lane = laneFor''',
    "generated post-dispatch drain",
)

source = replace_once(
    source,
    '''        if (!released) return;
    }

    if (event.target == MusicalEventTarget::Drums) {''',
    '''        if (!released) return;
        releasePendingChannelPanics();
    }

    if (event.target == MusicalEventTarget::Drums) {''',
    "lane pending release drain",
)

source = replace_once(
    source,
    '''            case MusicalEventType::AllNotesOff:
                break;
        }
        return;
    }

    switch (event.type) {''',
    '''            case MusicalEventType::AllNotesOff:
                break;
        }
        releasePendingChannelPanics();
        return;
    }

    switch (event.type) {''',
    "drum post-dispatch drain",
)

source = replace_once(
    source,
    '''        case MusicalEventType::AllNotesOff:
            break;
    }
}
''',
    '''        case MusicalEventType::AllNotesOff:
            break;
    }
    releasePendingChannelPanics();
}
''',
    "synth post-dispatch drain",
)

if "abandonedSmfChannels_" in source or "releaseAbandonedSmfChannels" in source:
    raise SystemExit("legacy SMF-only panic naming survived patch")
source_path.write_text(source)

clean_workflow = '''name: MIDI panic CC123 recovery

on:
  push:
    branches:
      - fix/20260905-midi-panic-cc123-recovery
  pull_request:

permissions:
  contents: read

jobs:
  focused:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Build focused panic recovery test
        run: |
          mkdir -p build/host-tests
          g++ -std=c++17 -Wall -Wextra -Werror -I. \\
            tests/test_midi_panic_cc123.cpp \\
            src/midi/usb_midi_output.cpp \\
            -o build/host-tests/test_midi_panic_cc123
      - name: Run focused panic recovery test
        run: build/host-tests/test_midi_panic_cc123
'''
Path(".github/workflows/midi-panic-cc123.yml").write_text(clean_workflow)

for temporary in (
    Path(".github/workflows/midi-panic-materialize.yml"),
    Path("tools/materialize_midi_panic_cc123.py"),
):
    if temporary.exists():
        temporary.unlink()
