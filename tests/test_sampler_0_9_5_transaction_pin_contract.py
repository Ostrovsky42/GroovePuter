#!/usr/bin/env python3
from dataclasses import dataclass
import unittest


@dataclass(frozen=True)
class Asset:
    asset_id: str
    decoded_bytes: int


@dataclass(frozen=True)
class TxResult:
    success: bool
    pads: dict[int, str]
    kit_id: str
    dirty_delta: int
    required_peak_bytes: int
    reason: str


class PinnedWarehouse:
    def __init__(self, capacity_bytes: int) -> None:
        self.capacity_bytes = capacity_bytes
        self.resident: dict[str, int] = {}
        self.pins: dict[str, int] = {}
        self.fail_preload: set[str] = set()

    def used_bytes(self) -> int:
        return sum(self.resident.values())

    def pin(self, asset_id: str) -> bool:
        if asset_id not in self.resident:
            return False
        self.pins[asset_id] = self.pins.get(asset_id, 0) + 1
        return True

    def unpin(self, asset_id: str) -> None:
        count = self.pins.get(asset_id, 0)
        if count <= 1:
            self.pins.pop(asset_id, None)
        else:
            self.pins[asset_id] = count - 1

    def pin_count(self, asset_id: str) -> int:
        return self.pins.get(asset_id, 0)

    def _evict_one(self) -> bool:
        candidates = sorted(
            asset_id for asset_id in self.resident
            if self.pin_count(asset_id) == 0
        )
        if not candidates:
            return False
        del self.resident[candidates[0]]
        return True

    def preload(self, asset: Asset) -> bool:
        if asset.asset_id in self.fail_preload:
            return False
        if asset.asset_id in self.resident:
            return self.resident[asset.asset_id] == asset.decoded_bytes
        if asset.decoded_bytes <= 0 or asset.decoded_bytes > self.capacity_bytes:
            return False

        while self.used_bytes() + asset.decoded_bytes > self.capacity_bytes:
            if not self._evict_one():
                return False

        self.resident[asset.asset_id] = asset.decoded_bytes
        return True


def _pin_unique(warehouse: PinnedWarehouse, asset_ids: set[str],
                tx_pins: set[str]) -> None:
    for asset_id in sorted(asset_ids):
        if asset_id in tx_pins:
            continue
        if warehouse.pin(asset_id):
            tx_pins.add(asset_id)


def _release_tx_pins(warehouse: PinnedWarehouse, tx_pins: set[str]) -> None:
    for asset_id in sorted(tx_pins):
        warehouse.unpin(asset_id)
    tx_pins.clear()


def load_transaction(*,
                     warehouse: PinnedWarehouse,
                     old_pads: dict[int, str],
                     old_kit_id: str,
                     target_pads: dict[int, Asset],
                     target_kit_id: str) -> TxResult:
    original_pads = dict(old_pads)
    if not target_kit_id:
        return TxResult(False, original_pads, old_kit_id, 0, 0,
                        "invalid-kit-id")
    if any(pad < 1 or pad > 8 for pad in target_pads):
        return TxResult(False, original_pads, old_kit_id, 0, 0,
                        "invalid-pad")

    unique_target: dict[str, Asset] = {}
    for asset in target_pads.values():
        if asset.decoded_bytes <= 0:
            return TxResult(False, original_pads, old_kit_id, 0, 0,
                            "invalid-size")
        existing = unique_target.get(asset.asset_id)
        if existing is not None and existing.decoded_bytes != asset.decoded_bytes:
            return TxResult(False, original_pads, old_kit_id, 0, 0,
                            "conflicting-target-size")
        unique_target[asset.asset_id] = asset

    tx_pins: set[str] = set()

    # Pin every resident sample reachable from the active kit. This is the
    # rollback guarantee: staging cannot evict PCM that the unchanged old pads
    # need if the transaction later fails.
    _pin_unique(warehouse, set(old_pads.values()), tx_pins)

    # Target assets that are already resident must also be pinned. Otherwise a
    # later preload could evict an earlier target before atomic publication.
    _pin_unique(warehouse, set(unique_target.keys()), tx_pins)

    protected_resident_bytes = sum(
        warehouse.resident[asset_id] for asset_id in tx_pins
    )
    missing_target = {
        asset_id: asset for asset_id, asset in unique_target.items()
        if asset_id not in warehouse.resident
    }
    required_peak = protected_resident_bytes + sum(
        asset.decoded_bytes for asset in missing_target.values()
    )

    if required_peak > warehouse.capacity_bytes:
        _release_tx_pins(warehouse, tx_pins)
        return TxResult(False, original_pads, old_kit_id, 0, required_peak,
                        "transaction-peak-exceeds-pool")

    for asset_id in sorted(missing_target):
        asset = missing_target[asset_id]
        if not warehouse.preload(asset):
            # Intentionally do not purge already staged unreferenced PCM. It is
            # warehouse cache, not active-kit semantics. Releasing transaction
            # pins makes it immediately LRU-eligible.
            _release_tx_pins(warehouse, tx_pins)
            return TxResult(False, original_pads, old_kit_id, 0, required_peak,
                            f"prepare-failed:{asset_id}")
        if not warehouse.pin(asset_id):
            _release_tx_pins(warehouse, tx_pins)
            return TxResult(False, original_pads, old_kit_id, 0, required_peak,
                            f"stage-pin-failed:{asset_id}")
        tx_pins.add(asset_id)

    committed_pads = {
        pad: asset.asset_id for pad, asset in sorted(target_pads.items())
    }

    # Atomic publication is represented by this single result construction.
    result = TxResult(True, committed_pads, target_kit_id, 1, required_peak,
                      "committed")
    _release_tx_pins(warehouse, tx_pins)
    return result


class TransactionPinContractTests(unittest.TestCase):
    def test_old_active_kit_is_pinned_during_prepare(self) -> None:
        warehouse = PinnedWarehouse(20 * 1024)
        warehouse.resident = {
            "old-kick": 4 * 1024,
            "old-snare": 4 * 1024,
            "garbage": 8 * 1024,
        }

        result = load_transaction(
            warehouse=warehouse,
            old_pads={1: "old-kick", 2: "old-snare"},
            old_kit_id="old.v1",
            target_pads={
                1: Asset("new-kick", 5 * 1024),
                2: Asset("new-snare", 5 * 1024),
            },
            target_kit_id="new.v1",
        )

        self.assertTrue(result.success)
        self.assertIn("old-kick", warehouse.resident)
        self.assertIn("old-snare", warehouse.resident)
        self.assertIn("new-kick", warehouse.resident)
        self.assertIn("new-snare", warehouse.resident)
        self.assertNotIn("garbage", warehouse.resident)
        self.assertEqual(warehouse.pins, {})

    def test_target_already_resident_is_pinned_before_other_preloads(self) -> None:
        warehouse = PinnedWarehouse(16 * 1024)
        warehouse.resident = {
            "old": 4 * 1024,
            "target-a": 4 * 1024,
            "garbage": 8 * 1024,
        }

        result = load_transaction(
            warehouse=warehouse,
            old_pads={1: "old"},
            old_kit_id="old.v1",
            target_pads={
                1: Asset("target-a", 4 * 1024),
                2: Asset("target-b", 4 * 1024),
            },
            target_kit_id="new.v1",
        )

        self.assertTrue(result.success)
        self.assertIn("target-a", warehouse.resident)
        self.assertIn("target-b", warehouse.resident)
        self.assertIn("old", warehouse.resident)
        self.assertNotIn("garbage", warehouse.resident)

    def test_peak_admission_rejects_before_staging(self) -> None:
        warehouse = PinnedWarehouse(32 * 1024)
        warehouse.resident = {
            "old-a": 13 * 1024,
            "old-b": 12 * 1024,
        }
        before = dict(warehouse.resident)

        result = load_transaction(
            warehouse=warehouse,
            old_pads={1: "old-a", 2: "old-b"},
            old_kit_id="old.v1",
            target_pads={
                1: Asset("new-a", 14 * 1024),
                2: Asset("new-b", 13 * 1024),
            },
            target_kit_id="new.v1",
        )

        self.assertFalse(result.success)
        self.assertEqual(result.reason, "transaction-peak-exceeds-pool")
        self.assertEqual(result.required_peak_bytes, 52 * 1024)
        self.assertEqual(result.pads, {1: "old-a", 2: "old-b"})
        self.assertEqual(result.kit_id, "old.v1")
        self.assertEqual(result.dirty_delta, 0)
        self.assertEqual(warehouse.resident, before)
        self.assertEqual(warehouse.pins, {})

    def test_prepare_failure_keeps_old_kit_but_may_leave_lru_cache(self) -> None:
        warehouse = PinnedWarehouse(24 * 1024)
        warehouse.resident = {
            "old-kick": 4 * 1024,
            "old-snare": 4 * 1024,
            "garbage": 8 * 1024,
        }
        warehouse.fail_preload.add("z-new-snare")

        result = load_transaction(
            warehouse=warehouse,
            old_pads={1: "old-kick", 2: "old-snare"},
            old_kit_id="old.v1",
            target_pads={
                1: Asset("a-new-kick", 4 * 1024),
                2: Asset("z-new-snare", 4 * 1024),
            },
            target_kit_id="new.v1",
        )

        self.assertFalse(result.success)
        self.assertEqual(result.pads, {1: "old-kick", 2: "old-snare"})
        self.assertEqual(result.kit_id, "old.v1")
        self.assertEqual(result.dirty_delta, 0)
        self.assertIn("old-kick", warehouse.resident)
        self.assertIn("old-snare", warehouse.resident)
        # Staged PCM is harmless warehouse cache after its transaction pin is
        # released. Production does not need a special unload/rollback API.
        self.assertIn("a-new-kick", warehouse.resident)
        self.assertEqual(warehouse.pin_count("a-new-kick"), 0)
        self.assertEqual(warehouse.pins, {})

    def test_external_audio_pin_can_make_prepare_fail_safely(self) -> None:
        warehouse = PinnedWarehouse(16 * 1024)
        warehouse.resident = {
            "old": 4 * 1024,
            "voice-held-unrelated": 8 * 1024,
        }
        self.assertTrue(warehouse.pin("voice-held-unrelated"))

        result = load_transaction(
            warehouse=warehouse,
            old_pads={1: "old"},
            old_kit_id="old.v1",
            target_pads={1: Asset("new", 8 * 1024)},
            target_kit_id="new.v1",
        )

        self.assertFalse(result.success)
        self.assertTrue(result.reason.startswith("prepare-failed:"))
        self.assertEqual(result.pads, {1: "old"})
        self.assertEqual(result.dirty_delta, 0)
        self.assertIn("old", warehouse.resident)
        self.assertEqual(warehouse.pin_count("old"), 0)
        self.assertEqual(warehouse.pin_count("voice-held-unrelated"), 1)

    def test_success_commit_releases_only_transaction_pins(self) -> None:
        warehouse = PinnedWarehouse(24 * 1024)
        warehouse.resident = {
            "old": 4 * 1024,
            "voice-held": 4 * 1024,
        }
        self.assertTrue(warehouse.pin("voice-held"))

        result = load_transaction(
            warehouse=warehouse,
            old_pads={1: "old"},
            old_kit_id="old.v1",
            target_pads={1: Asset("new", 4 * 1024)},
            target_kit_id="new.v1",
        )

        self.assertTrue(result.success)
        self.assertEqual(result.pads, {1: "new"})
        self.assertEqual(result.kit_id, "new.v1")
        self.assertEqual(result.dirty_delta, 1)
        self.assertEqual(warehouse.pin_count("old"), 0)
        self.assertEqual(warehouse.pin_count("new"), 0)
        self.assertEqual(warehouse.pin_count("voice-held"), 1)

    def test_old_voice_can_outlive_commit_through_its_own_pin(self) -> None:
        warehouse = PinnedWarehouse(16 * 1024)
        warehouse.resident = {"old": 4 * 1024}
        # Simulate an already-playing SamplerVoice handle.
        self.assertTrue(warehouse.pin("old"))

        result = load_transaction(
            warehouse=warehouse,
            old_pads={1: "old"},
            old_kit_id="old.v1",
            target_pads={1: Asset("new", 4 * 1024)},
            target_kit_id="new.v1",
        )

        self.assertTrue(result.success)
        self.assertEqual(result.pads, {1: "new"})
        self.assertIn("old", warehouse.resident)
        self.assertEqual(warehouse.pin_count("old"), 1)
        warehouse.unpin("old")
        self.assertEqual(warehouse.pin_count("old"), 0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
