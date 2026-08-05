#!/usr/bin/env python3
from pathlib import Path

ROOT = Path.cwd()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    profile_h = (ROOT / "src/midi/smf_track_route_profile.h").read_text(
        encoding="utf-8"
    )
    profile_cpp = (ROOT / "src/midi/smf_track_route_profile.cpp").read_text(
        encoding="utf-8"
    )
    runtime_h = (ROOT / "src/midi/smf_track_route_profile_runtime.h").read_text(
        encoding="utf-8"
    )
    routes = (ROOT / "src/midi/smf_track_output_route.h").read_text(
        encoding="utf-8"
    )
    service = (ROOT / "src/midi/smf_player_service.h").read_text(encoding="utf-8")
    player_h = (ROOT / "src/platform/cardputer_smf_player.h").read_text(
        encoding="utf-8"
    )
    player_cpp = (ROOT / "src/platform/cardputer_smf_player.cpp").read_text(
        encoding="utf-8"
    )
    platform_h = (ROOT / "src/platform/cardputer_smf_route_persistence.h").read_text(
        encoding="utf-8"
    )
    platform_cpp = (ROOT / "src/platform/cardputer_smf_route_persistence.cpp").read_text(
        encoding="utf-8"
    )
    display = (ROOT / "src/ui/miniacid_display.cpp").read_text(encoding="utf-8")
    hub = (ROOT / "src/ui/pages/sequencer_hub_page_midi.cpp").read_text(
        encoding="utf-8"
    )

    require(
        "kSmfTrackRouteProfileSlotCount = 16u" in profile_h
        and "kEncodedSize = 72u" in profile_h
        and "destinationChannels[kSmfTrackOutputRouteCapacity]" in profile_h,
        "route profiles must remain fixed-size and bounded",
    )
    require(
        "SmfTrackRouteFingerprint" in profile_h
        and "event.trackIndex" in profile_cpp
        and "event.event.tick" in profile_cpp
        and "event.event.kind" in profile_cpp
        and "event.event.channel" in profile_cpp
        and "event.event.data1" in profile_cpp
        and "event.event.data2" in profile_cpp,
        "file identity must be derived during the existing parsed-event pass",
    )
    require(
        "crc32(" in profile_cpp
        and "candidate.identity.matches(identity)" in profile_cpp
        and "SmfTrackRouteProfileLoadStatus::Stale" in profile_cpp
        and "SmfTrackRouteProfileLoadStatus::Corrupt" in profile_cpp,
        "corrupt or changed-file records must fail safely instead of applying stale routes",
    )
    require(
        "samePathSlot" in profile_cpp
        and "reusableSlot" in profile_cpp
        and "oldestSlot" in profile_cpp,
        "the fixed NVS bank must replace same-path, free, then oldest slots",
    )
    for forbidden in ("std::vector", "new ", "malloc", "free("):
        require(
            forbidden not in profile_h and forbidden not in profile_cpp,
            f"route profile codec must not allocate dynamically: {forbidden}",
        )

    require(
        "replaceDestinations(" in routes
        and "SmfSessionMutationGuard guard(generation)" in routes
        and "packedRoutes_[word].store(" in routes,
        "restored routes must be published as one generation-aware bounded mutation",
    )
    require(
        "portMUX_TYPE lock_" in runtime_h
        and "portENTER_CRITICAL(&runtime_.lock_)" in runtime_h
        and "portEXIT_CRITICAL(&runtime_.lock_)" in runtime_h
        and "requestLoad(" in runtime_h
        and "takeLoadRequest(" in runtime_h
        and "completeLoad(" in runtime_h
        and "requestSave(" in runtime_h
        and "takeSaveRequest(" in runtime_h,
        "player and persistence service must communicate through one fixed mailbox",
    )
    for forbidden in ("std::vector", "new ", "malloc", "free("):
        require(
            forbidden not in runtime_h,
            f"route persistence mailbox must not allocate dynamically: {forbidden}",
        )

    require(
        "persistTrackOutputRoutes(uint32_t generation)" in service
        and "persistTrackOutputRoutes(uint32_t generation) override" in player_h
        and "smfTrackRouteProfileRuntime().requestSave(generation)" in player_cpp,
        "a Hub save must publish a bounded request without NVS work in SmfPlayerTask",
    )
    require(
        "SmfTrackRouteFingerprint routeFingerprint(" in player_cpp
        and "routeFingerprint.observe(event);" in player_cpp
        and "smfTrackRouteProfileRuntime().requestLoad(" in player_cpp
        and "smfTrackRouteProfileRuntime().readyFor(routeGeneration)" in player_cpp
        and "ROUTES SYNCING" in player_cpp,
        "Cardputer load must publish identity after the existing pass and gate playback until restore",
    )
    for forbidden in ("Preferences", "putBytes", "getBytes", "smf_route_"):
        require(
            forbidden not in player_cpp,
            f"SmfPlayerTask must not execute NVS persistence: {forbidden}",
        )
    require(
        "PersistTrackRoutes" not in player_h
        and "PersistTrackRoutes" not in player_cpp,
        "route persistence must not be added to the SmfPlayerTask command queue",
    )

    require(
        "#ifdef ARDUINO" in platform_h
        and "serviceCardputerSmfRoutePersistence" in platform_h
        and "Preferences" in platform_cpp
        and "kSmfTrackRouteProfileSlotCount" in platform_cpp
        and "smf_route_%02u" in platform_cpp
        and "takeLoadRequest" in platform_cpp
        and "takeSaveRequest" in platform_cpp
        and "replaceDestinations" in platform_cpp,
        "the deferred platform persistence service must own the bounded NVS bank",
    )
    require(
        "cardputer_smf_route_persistence.h" in display
        and "serviceCardputerSmfRoutePersistence();" in display,
        "the root UI persistence loop must service route mailboxes outside SmfPlayerTask",
    )
    require(
        "service->persistTrackOutputRoutes(projection.generation)" in hub,
        "a successful Hub route edit must enqueue persistence for the current session",
    )
    for forbidden in ("Preferences", "SD.", "putBytes", "getBytes"):
        require(
            forbidden not in hub,
            f"HUB page must not own persistence or SD access: {forbidden}",
        )

    print("SMF route persistence source regressions: OK")


if __name__ == "__main__":
    main()
