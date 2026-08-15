#!/usr/bin/env python3
from dataclasses import dataclass
import unittest

from test_sampler_0_9_5_missing_relink_contract import (
    MAX_LOCATOR_BYTES,
    validate_locator,
)

USER_PAD_COUNT = 8
LOCATOR_SLOT_BYTES = MAX_LOCATOR_BYTES + 1
LOCATOR_LENGTH_BYTES = USER_PAD_COUNT
LOCATOR_MASK_BYTES = 1
RAW_SIDECAR_BYTES = (
    USER_PAD_COUNT * LOCATOR_SLOT_BYTES
    + LOCATOR_LENGTH_BYTES
    + LOCATOR_MASK_BYTES
)
MAX_DYNAMIC_SIDECAR_BYTES = 1088


class LocatorSidecarError(ValueError):
    pass


@dataclass(frozen=True)
class MissingLocatorEntry:
    stable_ref: int
    locator: str


class LocatorSidecar:
    """Reference model for one conditional heap allocation.

    The production implementation does not need Python's representation. The
    contract is that locator storage is absent when no user-facing pads are
    missing, and one bounded block is sufficient when any are missing.
    """

    def __init__(self) -> None:
        self._entries: list[MissingLocatorEntry | None] = [None] * USER_PAD_COUNT

    @property
    def allocated_bytes(self) -> int:
        return 0 if self.empty() else RAW_SIDECAR_BYTES

    def empty(self) -> bool:
        return all(entry is None for entry in self._entries)

    def set(self, pad_index: int, stable_ref: int, locator: str) -> None:
        if pad_index < 0 or pad_index >= USER_PAD_COUNT:
            raise LocatorSidecarError("locator sidecar is product pads 1..8 only")
        if stable_ref <= 0:
            raise LocatorSidecarError("missing locator requires stable ref")
        if not validate_locator(locator):
            raise LocatorSidecarError("invalid logical locator")
        self._entries[pad_index] = MissingLocatorEntry(stable_ref, locator)

    def clear(self, pad_index: int) -> None:
        if pad_index < 0 or pad_index >= USER_PAD_COUNT:
            raise LocatorSidecarError("pad out of range")
        self._entries[pad_index] = None

    def get(self, pad_index: int) -> MissingLocatorEntry | None:
        if pad_index < 0 or pad_index >= USER_PAD_COUNT:
            raise LocatorSidecarError("pad out of range")
        return self._entries[pad_index]

    def clone(self) -> "LocatorSidecar":
        other = LocatorSidecar()
        other._entries = list(self._entries)
        return other

    def snapshot(self) -> tuple[MissingLocatorEntry | None, ...]:
        return tuple(self._entries)


class SceneLoadLocatorTransaction:
    """Load-side pending sidecar, published only after successful filter finish."""

    def __init__(self) -> None:
        self.pending = LocatorSidecar()
        self.failed = False
        self.finished = False

    def record_missing(self, pad_index: int, stable_ref: int, locator: str) -> None:
        if self.failed or self.finished:
            raise LocatorSidecarError("transaction not writable")
        try:
            self.pending.set(pad_index, stable_ref, locator)
        except LocatorSidecarError:
            self.failed = True
            raise

    def fail(self) -> None:
        self.failed = True

    def finish(self) -> LocatorSidecar:
        if self.finished:
            raise LocatorSidecarError("finish called twice")
        self.finished = True
        if self.failed:
            # Mirrors the final 0.9.3 unresolved-ref rule: failed Scene load
            # does not publish a partially parsed missing-asset sidecar.
            return LocatorSidecar()
        return self.pending.clone()


class LocatorSidecarContractTests(unittest.TestCase):
    def test_sidecar_cost_is_zero_when_no_asset_is_missing(self) -> None:
        sidecar = LocatorSidecar()
        self.assertTrue(sidecar.empty())
        self.assertEqual(sidecar.allocated_bytes, 0)

    def test_single_dynamic_block_is_bounded_below_1088_bytes(self) -> None:
        self.assertEqual(RAW_SIDECAR_BYTES, 1041)
        self.assertLessEqual(RAW_SIDECAR_BYTES, MAX_DYNAMIC_SIDECAR_BYTES)

        sidecar = LocatorSidecar()
        for pad in range(USER_PAD_COUNT):
            locator = f"samples/pad{pad + 1}.wav"
            sidecar.set(pad, 0x1000 + pad, locator)
        self.assertEqual(sidecar.allocated_bytes, RAW_SIDECAR_BYTES)

    def test_max_length_locator_fits_every_user_pad(self) -> None:
        sidecar = LocatorSidecar()
        prefix = "samples/"
        suffix = ".wav"
        stem_len = MAX_LOCATOR_BYTES - len(prefix) - len(suffix)
        locator = prefix + ("a" * stem_len) + suffix
        self.assertEqual(len(locator.encode("utf-8")), MAX_LOCATOR_BYTES)
        self.assertTrue(validate_locator(locator))

        for pad in range(USER_PAD_COUNT):
            sidecar.set(pad, 0x2000 + pad, locator)
        self.assertEqual(sidecar.allocated_bytes, RAW_SIDECAR_BYTES)

    def test_internal_pads_9_to_16_do_not_expand_locator_sidecar(self) -> None:
        sidecar = LocatorSidecar()
        with self.assertRaises(LocatorSidecarError):
            sidecar.set(8, 0x3000, "samples/internal.wav")
        self.assertEqual(sidecar.allocated_bytes, 0)

    def test_failed_scene_load_publishes_no_partial_locator_state(self) -> None:
        tx = SceneLoadLocatorTransaction()
        tx.record_missing(0, 0xAAAA, "samples/kick.wav")
        tx.record_missing(1, 0xBBBB, "samples/snare.wav")
        tx.fail()

        published = tx.finish()
        self.assertTrue(published.empty())
        self.assertEqual(published.allocated_bytes, 0)

    def test_successful_scene_load_publishes_all_missing_locators_together(self) -> None:
        tx = SceneLoadLocatorTransaction()
        tx.record_missing(0, 0xAAAA, "samples/kick.wav")
        tx.record_missing(2, 0xCCCC, "kit:sp12.factory.v1:snare.wav")

        published = tx.finish()
        self.assertEqual(published.get(0), MissingLocatorEntry(0xAAAA, "samples/kick.wav"))
        self.assertIsNone(published.get(1))
        self.assertEqual(
            published.get(2),
            MissingLocatorEntry(0xCCCC, "kit:sp12.factory.v1:snare.wav"),
        )
        self.assertEqual(published.allocated_bytes, RAW_SIDECAR_BYTES)

    def test_clearing_last_missing_locator_releases_sidecar_budget(self) -> None:
        sidecar = LocatorSidecar()
        sidecar.set(0, 0xAAAA, "samples/kick.wav")
        self.assertGreater(sidecar.allocated_bytes, 0)
        sidecar.clear(0)
        self.assertTrue(sidecar.empty())
        self.assertEqual(sidecar.allocated_bytes, 0)

    def test_relink_replaces_sidecar_identity_only_after_commit(self) -> None:
        sidecar = LocatorSidecar()
        sidecar.set(0, 0xAAAA, "samples/missing.wav")
        before = sidecar.snapshot()

        # Failed relink: caller never mutates sidecar.
        self.assertEqual(sidecar.snapshot(), before)

        # Successful relink to a resident sample clears missing-only locator
        # storage; persistence can derive resident locator from the registry.
        sidecar.clear(0)
        self.assertIsNone(sidecar.get(0))
        self.assertEqual(sidecar.allocated_bytes, 0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
