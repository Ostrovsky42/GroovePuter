#!/usr/bin/env python3
from dataclasses import dataclass, replace
from typing import Dict, Optional, Tuple
import unittest

MAX_LOCATOR_BYTES = 128


def _validate_relative_path(path: str) -> bool:
    if not path or len(path.encode("utf-8")) > MAX_LOCATOR_BYTES:
        return False
    if path.startswith("/") or path.endswith("/") or "\\" in path:
        return False
    parts = path.split("/")
    if any(part in ("", ".", "..") for part in parts):
        return False
    return True


def validate_locator(locator: str) -> bool:
    if not locator or len(locator.encode("utf-8")) > MAX_LOCATOR_BYTES:
        return False

    if locator.startswith("samples/"):
        return _validate_relative_path(locator[len("samples/"):])

    if locator.startswith("kit:"):
        rest = locator[len("kit:"):]
        if ":" not in rest:
            return False
        kit_id, asset_path = rest.split(":", 1)
        if not kit_id or len(kit_id.encode("utf-8")) > 48:
            return False
        if any(ch.isspace() for ch in kit_id):
            return False
        if any(ch in "/\\:" for ch in kit_id):
            return False
        return _validate_relative_path(asset_path)

    return False


@dataclass(frozen=True)
class PersistedPad:
    runtime_id: int
    stable_ref: int
    locator: str
    volume: float = 1.0
    pitch: float = 1.0
    start_frame: int = 0
    end_frame: int = 0
    loop: bool = False
    reverse: bool = False
    choke_group: int = 0

    @property
    def missing(self) -> bool:
        return self.stable_ref != 0 and self.runtime_id == 0


class StableRegistry:
    def __init__(self) -> None:
        self._runtime_by_ref: Dict[int, int] = {}
        self._locator_by_ref: Dict[int, str] = {}

    def publish(self, stable_ref: int, runtime_id: int, locator: str) -> None:
        if stable_ref <= 0 or runtime_id <= 0 or not validate_locator(locator):
            raise ValueError("invalid registry binding")
        self._runtime_by_ref[stable_ref] = runtime_id
        self._locator_by_ref[stable_ref] = locator

    def remove(self, stable_ref: int) -> None:
        self._runtime_by_ref.pop(stable_ref, None)
        self._locator_by_ref.pop(stable_ref, None)

    def resolve(self, stable_ref: int) -> int:
        return self._runtime_by_ref.get(stable_ref, 0)

    def locator(self, stable_ref: int) -> Optional[str]:
        return self._locator_by_ref.get(stable_ref)


@dataclass(frozen=True)
class RelinkCandidate:
    stable_ref: int
    runtime_id: int
    locator: str


@dataclass(frozen=True)
class RelinkResult:
    pad: PersistedPad
    scene_revision_delta: int
    committed: bool
    failure: str = ""


def restore_from_persistence(saved: PersistedPad,
                             registry: StableRegistry) -> PersistedPad:
    if saved.stable_ref == 0:
        return saved
    if not validate_locator(saved.locator):
        raise ValueError("invalid persisted locator")

    runtime_id = registry.resolve(saved.stable_ref)
    if runtime_id == 0:
        # Missing is a first-class storage state. Stable identity, locator and
        # all pad parameters survive while realtime identity becomes silent.
        return replace(saved, runtime_id=0)
    return replace(saved, runtime_id=runtime_id)


def save_for_persistence(runtime: PersistedPad,
                         sidecar: PersistedPad) -> PersistedPad:
    if runtime.runtime_id != 0:
        if runtime.stable_ref == 0 or not validate_locator(runtime.locator):
            raise ValueError("resident sample lacks stable persistence identity")
        return runtime

    # 0.9.3 already proved that a missing stable ref must survive Save. 0.9.5-D
    # extends that rule to a reversible locator without expanding realtime ABI.
    if sidecar.stable_ref != 0:
        if not validate_locator(sidecar.locator):
            raise ValueError("missing sidecar locator invalid")
        return replace(sidecar,
                       volume=runtime.volume,
                       pitch=runtime.pitch,
                       start_frame=runtime.start_frame,
                       end_frame=runtime.end_frame,
                       loop=runtime.loop,
                       reverse=runtime.reverse,
                       choke_group=runtime.choke_group,
                       runtime_id=0)
    return runtime


def relink_transaction(current: PersistedPad,
                       candidate: RelinkCandidate,
                       *,
                       inspect_ok: bool,
                       admission_ok: bool,
                       preload_ok: bool) -> RelinkResult:
    if not validate_locator(candidate.locator):
        return RelinkResult(current, 0, False, "invalid-locator")
    if candidate.stable_ref <= 0 or candidate.runtime_id <= 0:
        return RelinkResult(current, 0, False, "invalid-identity")
    if not inspect_ok:
        return RelinkResult(current, 0, False, "inspect")
    if not admission_ok:
        return RelinkResult(current, 0, False, "admission")
    if not preload_ok:
        return RelinkResult(current, 0, False, "preload")

    committed = replace(current,
                        runtime_id=candidate.runtime_id,
                        stable_ref=candidate.stable_ref,
                        locator=candidate.locator)
    return RelinkResult(committed, 1, True)


class MissingRelinkContractTests(unittest.TestCase):
    def test_loose_and_kit_locators_are_reversible_and_bounded(self) -> None:
        self.assertTrue(validate_locator("samples/kick.wav"))
        self.assertTrue(validate_locator("samples/drums/kick.wav"))
        self.assertTrue(validate_locator("kit:sp12.factory.v1:snare.wav"))
        self.assertTrue(validate_locator("kit:my-kit.v2:drums/oh.wav"))

        self.assertFalse(validate_locator("/samples/kick.wav"))
        self.assertFalse(validate_locator("samples/../scene.json"))
        self.assertFalse(validate_locator("samples/drums\\kick.wav"))
        self.assertFalse(validate_locator("kit:sp12.factory.v1:../snare.wav"))
        self.assertFalse(validate_locator("kit:bad/id:snare.wav"))
        self.assertFalse(validate_locator("kit:bad id:snare.wav"))
        self.assertFalse(validate_locator("unknown:kick.wav"))
        self.assertFalse(validate_locator("samples/" + "a" * 129))

    def test_missing_asset_keeps_ref_locator_and_parameters(self) -> None:
        saved = PersistedPad(
            runtime_id=123,
            stable_ref=0xAABBCCDDEEFF0011,
            locator="samples/snare.wav",
            volume=0.75,
            pitch=0.9,
            start_frame=100,
            end_frame=900,
            loop=True,
            reverse=True,
            choke_group=2,
        )
        registry = StableRegistry()

        restored = restore_from_persistence(saved, registry)
        self.assertTrue(restored.missing)
        self.assertEqual(restored.stable_ref, saved.stable_ref)
        self.assertEqual(restored.locator, saved.locator)
        self.assertEqual(restored.volume, 0.75)
        self.assertEqual(restored.pitch, 0.9)
        self.assertEqual(restored.start_frame, 100)
        self.assertEqual(restored.end_frame, 900)
        self.assertTrue(restored.loop)
        self.assertTrue(restored.reverse)
        self.assertEqual(restored.choke_group, 2)

    def test_save_while_missing_preserves_identity_and_locator(self) -> None:
        sidecar = PersistedPad(
            runtime_id=0,
            stable_ref=0x1111222233334444,
            locator="kit:sp12.factory.v1:snare.wav",
            volume=0.8,
            pitch=1.0,
        )
        runtime = replace(sidecar, volume=0.65, pitch=1.25, reverse=True)

        saved = save_for_persistence(runtime, sidecar)
        self.assertEqual(saved.stable_ref, sidecar.stable_ref)
        self.assertEqual(saved.locator, sidecar.locator)
        self.assertEqual(saved.runtime_id, 0)
        self.assertEqual(saved.volume, 0.65)
        self.assertEqual(saved.pitch, 1.25)
        self.assertTrue(saved.reverse)

    def test_missing_ref_auto_resolves_when_original_asset_returns(self) -> None:
        ref = 0x0102030405060708
        saved = PersistedPad(0, ref, "samples/kick.wav")
        registry = StableRegistry()

        missing = restore_from_persistence(saved, registry)
        self.assertTrue(missing.missing)

        registry.publish(ref, 90210, "samples/kick.wav")
        restored = restore_from_persistence(saved, registry)
        self.assertFalse(restored.missing)
        self.assertEqual(restored.runtime_id, 90210)
        self.assertEqual(restored.stable_ref, ref)
        self.assertEqual(restored.locator, "samples/kick.wav")

    def test_kit_locator_survives_physical_directory_rename(self) -> None:
        # The persisted locator uses stable kit identity, not directory name.
        locator_before = "kit:sp12.factory.v1:snare.wav"
        locator_after_directory_rename = "kit:sp12.factory.v1:snare.wav"
        self.assertEqual(locator_before, locator_after_directory_rename)
        self.assertTrue(validate_locator(locator_after_directory_rename))

    def test_failed_relink_is_bit_for_bit_non_mutating(self) -> None:
        current = PersistedPad(
            runtime_id=0,
            stable_ref=0xAAAA,
            locator="samples/missing.wav",
            volume=0.7,
            pitch=1.1,
            start_frame=42,
            end_frame=4242,
            loop=True,
            reverse=True,
            choke_group=3,
        )
        candidate = RelinkCandidate(
            stable_ref=0xBBBB,
            runtime_id=77,
            locator="samples/replacement.wav",
        )

        for kwargs, expected_failure in (
            (dict(inspect_ok=False, admission_ok=True, preload_ok=True), "inspect"),
            (dict(inspect_ok=True, admission_ok=False, preload_ok=True), "admission"),
            (dict(inspect_ok=True, admission_ok=True, preload_ok=False), "preload"),
        ):
            result = relink_transaction(current, candidate, **kwargs)
            self.assertFalse(result.committed)
            self.assertEqual(result.failure, expected_failure)
            self.assertEqual(result.scene_revision_delta, 0)
            self.assertEqual(result.pad, current)

    def test_successful_relink_commits_identity_locator_once(self) -> None:
        current = PersistedPad(
            runtime_id=0,
            stable_ref=0xAAAA,
            locator="samples/missing.wav",
            volume=0.7,
            pitch=1.1,
            start_frame=42,
            end_frame=4242,
            loop=True,
            reverse=False,
            choke_group=3,
        )
        candidate = RelinkCandidate(
            stable_ref=0xBBBBCCCCDDDDEEEE,
            runtime_id=77,
            locator="kit:sp12.factory.v1:snare.wav",
        )

        result = relink_transaction(
            current,
            candidate,
            inspect_ok=True,
            admission_ok=True,
            preload_ok=True,
        )

        self.assertTrue(result.committed)
        self.assertEqual(result.scene_revision_delta, 1)
        self.assertEqual(result.pad.runtime_id, 77)
        self.assertEqual(result.pad.stable_ref, candidate.stable_ref)
        self.assertEqual(result.pad.locator, candidate.locator)
        # Relink changes physical identity only; musical pad parameters survive.
        self.assertEqual(result.pad.volume, current.volume)
        self.assertEqual(result.pad.pitch, current.pitch)
        self.assertEqual(result.pad.start_frame, current.start_frame)
        self.assertEqual(result.pad.end_frame, current.end_frame)
        self.assertEqual(result.pad.loop, current.loop)
        self.assertEqual(result.pad.reverse, current.reverse)
        self.assertEqual(result.pad.choke_group, current.choke_group)

    def test_invalid_relink_locator_fails_before_commit(self) -> None:
        current = PersistedPad(0, 0xAAAA, "samples/missing.wav")
        candidate = RelinkCandidate(0xBBBB, 77, "samples/../scene.json")
        result = relink_transaction(
            current,
            candidate,
            inspect_ok=True,
            admission_ok=True,
            preload_ok=True,
        )
        self.assertFalse(result.committed)
        self.assertEqual(result.failure, "invalid-locator")
        self.assertEqual(result.scene_revision_delta, 0)
        self.assertEqual(result.pad, current)


if __name__ == "__main__":
    unittest.main(verbosity=2)
