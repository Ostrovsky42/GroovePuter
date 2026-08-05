#!/usr/bin/env python3
from pathlib import Path

ROOT = Path.cwd()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    page = (ROOT / "src/ui/pages/sequencer_hub_page_midi.cpp").read_text(
        encoding="utf-8"
    )
    header = (ROOT / "src/ui/pages/sequencer_hub_page.h").read_text(
        encoding="utf-8"
    )
    display = (ROOT / "src/ui/miniacid_display.h").read_text(
        encoding="utf-8"
    )
    makefile = (ROOT / "platform_sdl/Makefile").read_text(encoding="utf-8")

    require(
        "class SequencerHubPage final : public SequencerHubPageBase" in header,
        "HUB MIDI must remain a thin projection over the existing Hub page",
    )
    require(
        "bool midiOverview_{false};" in header
        and "bool midiReturnToPlayer_{false};" in header
        and "uint8_t midiSelected_{0};" in header
        and "uint8_t midiScroll_{0};" in header
        and "uint32_t midiGeneration_{0};" in header,
        "HUB MIDI state must remain page-local and bounded",
    )
    for forbidden in ("std::vector", "std::array", "SmfPlayerSnapshot player_", "static Smf"):
        require(
            forbidden not in header,
            f"HUB MIDI header must not retain runtime snapshot/cache state: {forbidden}",
        )

    require(
        "GROOVEPUTER_SEQUENCER_HUB_WRAPPER_CONSUMER" in display,
        "MiniAcidDisplay must instantiate the public HUB MIDI wrapper",
    )
    require(
        makefile.count("../src/ui/pages/sequencer_hub_page.cpp") == 1
        and makefile.count("../src/ui/pages/sequencer_hub_page_midi.cpp") == 1,
        "SDL must compile both the existing Hub implementation and MIDI projection once",
    )

    for snapshot_call in (
        "smfStructuralInspectorState().snapshot()",
        "smfTrackInspectorState().snapshot()",
        "smfTrackMuteState().snapshot()",
        "service->snapshot()",
    ):
        require(
            snapshot_call in page,
            f"HUB MIDI must project existing snapshots: {snapshot_call}",
        )

    require(
        "smfSessionGeneration()" in page
        and "smfSnapshotGenerationsMatch(" in page
        and "SYNCING" in page,
        "HUB MIDI must reject stale snapshots and expose an explicit SYNCING state",
    )
    require(
        "state.toggleTrack(track, projection.generation)" in page,
        "HUB MIDI mute must use the generation-aware physical-track mute path",
    )
    require(
        "smfTrackMuteState().clear(projection.generation)" in page,
        "HUB MIDI All On must use the generation-aware existing mute state",
    )
    require(
        "smfTrackMuteState().selectTrack(" in page
        and "projection.generation" in page,
        "HUB MIDI row selection must follow the existing generation-aware physical track",
    )

    shortcut_pos = page.index("const bool hubShortcut")
    modifier_pos = page.index("if (event.alt || event.ctrl || event.meta)")
    require(
        shortcut_pos < modifier_pos
        and "event.key == 'h'" in page[shortcut_pos:modifier_pos]
        and "UIInput::isBack(event)" in page[shortcut_pos:modifier_pos]
        and "returnFromMidiOverview();" in page[shortcut_pos:modifier_pos],
        "H and Escape/Back must return to the origin before Cardputer meta filtering",
    )
    require(
        "drawFormStrip(" in page
        and "layer.form[section]" in page
        and "currentFormSection(player.bar)" in page
        and "H/ESC Player  1-9 Mute" in page,
        "HUB MIDI must use a Hub-like arrangement strip with a visible play section",
    )
    require(
        "G%s SW%u" not in page
        and "N%u.%u A%u%%" not in page
        and "P Player U Mixer I Chans" not in page
        and "event.key == 'u'" not in page
        and "event.key == 'i'" not in page,
        "HUB MIDI must not expose raw inspector metrics or duplicate U/I panel navigation",
    )
    require(
        "formatTrackChannel(" in page
        and "loopLabel(" in page
        and "TRACK %02u  %s  %s" in page,
        "selected-layer summary must use readable channel and loop labels",
    )
    require(
        "GROOVEPUTER_LEFT" in page
        and "GROOVEPUTER_RIGHT" in page
        and "kVisibleMidiRows" in page,
        "HUB MIDI must page through long layer lists without adding retained storage",
    )
    require(
        "event.key == ' '" in page
        and "MIDI TRANSPORT: PLAYER" in page,
        "Space must be consumed with an explicit Player-owned transport message",
    )
    require(
        "void SequencerHubPage::onEnter(int context)" in page
        and "PlayerHubNavigation::kOpenMidiFromPlayerContext" in page
        and "requestPageTransition(PlayerHubNavigation::kPlayerPage)" in page,
        "HUB MIDI must support the bounded Player round-trip without owning transport",
    )

    for forbidden in (
        "SD.",
        "SmfFileIndexer",
        "SmfScheduler",
        "loadFile(",
        "loadSelected(",
        "seek(",
        "restart(",
        "->play(",
        "->pause(",
        "->stop(",
        "tud_midi",
        "TinyUSB",
        "USBMIDI",
    ):
        require(
            forbidden not in page,
            f"HUB MIDI must not acquire file, transport, scheduler or USB ownership: {forbidden}",
        )

    require(
        "mode_ == Mode::OVERVIEW" in page,
        "MIDI projection must only replace the Hub overview, not detail editing",
    )
    require(
        "SequencerHubPageBase::draw(gfx)" in page
        and "SequencerHubPageBase::handleEvent(event)" in page,
        "Internal Hub behavior must remain delegated to the existing implementation",
    )

    print("HUB MIDI Stage 1C source regressions: OK")


if __name__ == "__main__":
    main()
