#!/usr/bin/env python3
import copy
import re
import unittest
from dataclasses import dataclass

KIT_ID_RE = re.compile(r"^[a-z0-9][a-z0-9._-]{0,63}$")
MAX_KIT_NAME_BYTES = 32
MAX_ASSET_LOCATOR_BYTES = 96
USER_PAD_MIN = 1
USER_PAD_MAX = 8


class KitContractError(ValueError):
    pass


@dataclass(frozen=True)
class KitAsset:
    pad: int
    locator: str


@dataclass(frozen=True)
class KitManifest:
    schema: int
    kit_id: str
    name: str
    assets: tuple[KitAsset, ...]


@dataclass(frozen=True)
class AssetPlan:
    asset_id: str
    decoded_bytes: int


@dataclass(frozen=True)
class TransactionResult:
    success: bool
    pads: dict[int, str]
    active_kit_id: str
    scene_dirty_count: int
    required_peak_bytes: int
    reason: str


def _utf8_len(value: str) -> int:
    return len(value.encode("utf-8"))


def validate_asset_locator(locator: object) -> str:
    if not isinstance(locator, str):
        raise KitContractError("asset locator must be a string")
    if not locator or _utf8_len(locator) > MAX_ASSET_LOCATOR_BYTES:
        raise KitContractError("asset locator length out of range")
    if "\x00" in locator or any(ord(ch) < 0x20 for ch in locator):
        raise KitContractError("asset locator contains control characters")
    if locator.startswith("/"):
        raise KitContractError("absolute asset path rejected")
    if "\\" in locator:
        raise KitContractError("backslash path aliases rejected")

    components = locator.split("/")
    if any(part in ("", ".", "..") for part in components):
        raise KitContractError("empty/dot/path-traversal segment rejected")
    if not locator.lower().endswith(".wav"):
        raise KitContractError("kit asset must reference a WAV file")
    return locator


def validate_manifest(document: object) -> KitManifest:
    if not isinstance(document, dict):
        raise KitContractError("manifest must be an object")
    if set(document.keys()) != {"schema", "id", "name", "pads"}:
        raise KitContractError("manifest v1 has an exact top-level field set")
    if document["schema"] != 1:
        raise KitContractError("unsupported kit schema")

    kit_id = document["id"]
    if not isinstance(kit_id, str) or not KIT_ID_RE.fullmatch(kit_id):
        raise KitContractError("invalid stable kit id")

    name = document["name"]
    if not isinstance(name, str) or not name or _utf8_len(name) > MAX_KIT_NAME_BYTES:
        raise KitContractError("invalid kit display name")

    pads = document["pads"]
    if not isinstance(pads, list) or not pads or len(pads) > USER_PAD_MAX:
        raise KitContractError("pads must contain 1..8 assignments")

    seen_pads: set[int] = set()
    assets: list[KitAsset] = []
    for entry in pads:
        if not isinstance(entry, dict) or set(entry.keys()) != {"pad", "file"}:
            raise KitContractError("pad entry must contain exactly pad/file")
        pad = entry["pad"]
        if isinstance(pad, bool) or not isinstance(pad, int):
            raise KitContractError("pad must be an integer")
        if pad < USER_PAD_MIN or pad > USER_PAD_MAX:
            raise KitContractError("pad outside recovered user range")
        if pad in seen_pads:
            raise KitContractError("duplicate pad assignment")
        seen_pads.add(pad)
        assets.append(KitAsset(pad=pad, locator=validate_asset_locator(entry["file"])))

    return KitManifest(
        schema=1,
        kit_id=kit_id,
        name=name,
        assets=tuple(assets),
    )


class FakeWarehouse:
    def __init__(self, capacity_bytes: int, max_slots: int = 64) -> None:
        self.capacity_bytes = capacity_bytes
        self.max_slots = max_slots
        self.resident: dict[str, int] = {}

    def used_bytes(self) -> int:
        return sum(self.resident.values())

    def preload(self, asset: AssetPlan, protected: set[str],
                fail_assets: set[str]) -> tuple[bool, bool]:
        if asset.asset_id in fail_assets:
            return False, False
        if asset.asset_id in self.resident:
            if self.resident[asset.asset_id] != asset.decoded_bytes:
                return False, False
            return True, False
        if asset.decoded_bytes <= 0 or asset.decoded_bytes > self.capacity_bytes:
            return False, False

        while self.used_bytes() + asset.decoded_bytes > self.capacity_bytes:
            candidates = sorted(
                asset_id for asset_id in self.resident
                if asset_id not in protected
            )
            if not candidates:
                return False, False
            del self.resident[candidates[0]]

        if len(self.resident) >= self.max_slots:
            candidates = sorted(
                asset_id for asset_id in self.resident
                if asset_id not in protected
            )
            if not candidates:
                return False, False
            del self.resident[candidates[0]]

        self.resident[asset.asset_id] = asset.decoded_bytes
        return True, True


def load_kit_transaction(*, old_pads: dict[int, str], old_kit_id: str,
                         new_kit_id: str, new_pad_assets: dict[int, AssetPlan],
                         warehouse: FakeWarehouse,
                         fail_assets: set[str] | None = None) -> TransactionResult:
    fail_assets = fail_assets or set()
    original_pads = copy.deepcopy(old_pads)

    if not KIT_ID_RE.fullmatch(new_kit_id):
        return TransactionResult(
            False, original_pads, old_kit_id, 0, 0, "invalid kit id"
        )
    if any(pad < USER_PAD_MIN or pad > USER_PAD_MAX for pad in new_pad_assets):
        return TransactionResult(
            False, original_pads, old_kit_id, 0, 0, "pad outside user range"
        )
    if any(asset.decoded_bytes <= 0 for asset in new_pad_assets.values()):
        return TransactionResult(
            False, original_pads, old_kit_id, 0, 0, "invalid decoded size"
        )

    unique_new: dict[str, AssetPlan] = {}
    for asset in new_pad_assets.values():
        existing = unique_new.get(asset.asset_id)
        if existing is not None and existing.decoded_bytes != asset.decoded_bytes:
            return TransactionResult(
                False, original_pads, old_kit_id, 0, 0,
                "same asset id resolved to conflicting sizes",
            )
        unique_new[asset.asset_id] = asset

    protected = {
        asset_id for asset_id in old_pads.values()
        if asset_id in warehouse.resident
    }
    protected_bytes = sum(warehouse.resident[asset_id] for asset_id in protected)
    new_not_resident = {
        asset_id: asset for asset_id, asset in unique_new.items()
        if asset_id not in warehouse.resident
    }
    required_peak = protected_bytes + sum(
        asset.decoded_bytes for asset in new_not_resident.values()
    )
    required_slots = len(protected | set(unique_new.keys()))

    if required_peak > warehouse.capacity_bytes:
        return TransactionResult(
            False, original_pads, old_kit_id, 0, required_peak,
            "protected old kit plus staged new kit exceeds pool",
        )
    if required_slots > warehouse.max_slots:
        return TransactionResult(
            False, original_pads, old_kit_id, 0, required_peak,
            "protected/staged sample count exceeds slots",
        )

    resident_before_prepare = set(warehouse.resident.keys())
    newly_staged: set[str] = set()

    for asset_id in sorted(unique_new):
        ok, was_new = warehouse.preload(
            unique_new[asset_id], protected, fail_assets
        )
        if not ok:
            for staged_id in newly_staged:
                if staged_id not in resident_before_prepare:
                    warehouse.resident.pop(staged_id, None)
            return TransactionResult(
                False, original_pads, old_kit_id, 0, required_peak,
                f"prepare failed for {asset_id}",
            )
        if was_new:
            newly_staged.add(asset_id)

    committed_pads = {
        pad: asset.asset_id for pad, asset in sorted(new_pad_assets.items())
    }
    return TransactionResult(
        True, committed_pads, new_kit_id, 1, required_peak, "committed"
    )


class Kit095ResearchContract(unittest.TestCase):
    def test_accepts_canonical_manifest_and_identity_is_directory_independent(self) -> None:
        document = {
            "schema": 1,
            "id": "sp12.factory.v1",
            "name": "SP12",
            "pads": [
                {"pad": 1, "file": "kick.wav"},
                {"pad": 2, "file": "drums/snare.WAV"},
            ],
        }
        manifest = validate_manifest(document)
        self.assertEqual(manifest.kit_id, "sp12.factory.v1")
        self.assertEqual(manifest.assets[1].locator, "drums/snare.WAV")
        self.assertNotIn("SP12", manifest.kit_id)

    def test_rejects_noncanonical_manifest_shape(self) -> None:
        valid = {
            "schema": 1,
            "id": "mykit.v1",
            "name": "MyKit",
            "pads": [{"pad": 1, "file": "kick.wav"}],
        }
        bad_documents = [
            {**valid, "schema": 2},
            {**valid, "id": "My Kit"},
            {**valid, "id": ""},
            {**valid, "name": ""},
            {**valid, "extra": True},
            {**valid, "pads": []},
        ]
        for document in bad_documents:
            with self.subTest(document=document):
                with self.assertRaises(KitContractError):
                    validate_manifest(document)

    def test_rejects_absolute_traversal_dot_empty_and_backslash_locators(self) -> None:
        bad = [
            "/kick.wav",
            "../kick.wav",
            "drums/../kick.wav",
            "./kick.wav",
            "drums//kick.wav",
            "drums\\kick.wav",
            "kick.aiff",
        ]
        for locator in bad:
            with self.subTest(locator=locator):
                with self.assertRaises(KitContractError):
                    validate_asset_locator(locator)

    def test_rejects_duplicate_or_out_of_range_pads(self) -> None:
        base = {"schema": 1, "id": "kit.v1", "name": "Kit"}
        cases = [
            [{"pad": 1, "file": "a.wav"}, {"pad": 1, "file": "b.wav"}],
            [{"pad": 0, "file": "a.wav"}],
            [{"pad": 9, "file": "a.wav"}],
            [{"pad": True, "file": "a.wav"}],
        ]
        for pads in cases:
            with self.subTest(pads=pads):
                with self.assertRaises(KitContractError):
                    validate_manifest({**base, "pads": pads})

    def test_allows_multiple_pads_to_share_one_asset(self) -> None:
        manifest = validate_manifest({
            "schema": 1,
            "id": "shared.v1",
            "name": "Shared",
            "pads": [
                {"pad": 1, "file": "hit.wav"},
                {"pad": 2, "file": "hit.wav"},
            ],
        })
        self.assertEqual(manifest.assets[0].locator, manifest.assets[1].locator)

    def test_transaction_commits_all_pads_once_after_prepare(self) -> None:
        warehouse = FakeWarehouse(32 * 1024)
        warehouse.resident = {"old-kick": 4096, "old-snare": 4096, "warehouse-x": 4096}
        old_pads = {1: "old-kick", 2: "old-snare"}
        new_assets = {
            1: AssetPlan("new-kick", 4096),
            2: AssetPlan("new-snare", 4096),
            3: AssetPlan("new-hat", 2048),
        }
        result = load_kit_transaction(
            old_pads=old_pads,
            old_kit_id="old.v1",
            new_kit_id="new.v1",
            new_pad_assets=new_assets,
            warehouse=warehouse,
        )
        self.assertTrue(result.success)
        self.assertEqual(result.scene_dirty_count, 1)
        self.assertEqual(result.active_kit_id, "new.v1")
        self.assertEqual(result.pads, {1: "new-kick", 2: "new-snare", 3: "new-hat"})
        self.assertIn("old-kick", warehouse.resident)
        self.assertIn("old-snare", warehouse.resident)

    def test_safe_admission_rejects_25k_old_plus_27k_new_in_32k_pool(self) -> None:
        warehouse = FakeWarehouse(32 * 1024)
        warehouse.resident = {"old-a": 13 * 1024, "old-b": 12 * 1024}
        old_pads = {1: "old-a", 2: "old-b"}
        original_resident = copy.deepcopy(warehouse.resident)
        result = load_kit_transaction(
            old_pads=old_pads,
            old_kit_id="old.v1",
            new_kit_id="new.v1",
            new_pad_assets={
                1: AssetPlan("new-a", 14 * 1024),
                2: AssetPlan("new-b", 13 * 1024),
            },
            warehouse=warehouse,
        )
        self.assertFalse(result.success)
        self.assertEqual(result.required_peak_bytes, 52 * 1024)
        self.assertEqual(result.pads, old_pads)
        self.assertEqual(result.active_kit_id, "old.v1")
        self.assertEqual(result.scene_dirty_count, 0)
        self.assertEqual(warehouse.resident, original_resident)

    def test_prepare_failure_rolls_back_staged_assets_and_keeps_old_kit(self) -> None:
        warehouse = FakeWarehouse(32 * 1024)
        warehouse.resident = {
            "old-kick": 4096,
            "old-snare": 4096,
            "warehouse-garbage": 12 * 1024,
        }
        old_pads = {1: "old-kick", 2: "old-snare"}
        result = load_kit_transaction(
            old_pads=old_pads,
            old_kit_id="old.v1",
            new_kit_id="new.v1",
            new_pad_assets={
                1: AssetPlan("a-new-kick", 8 * 1024),
                2: AssetPlan("z-new-snare", 8 * 1024),
            },
            warehouse=warehouse,
            fail_assets={"z-new-snare"},
        )
        self.assertFalse(result.success)
        self.assertEqual(result.pads, old_pads)
        self.assertEqual(result.scene_dirty_count, 0)
        self.assertIn("old-kick", warehouse.resident)
        self.assertIn("old-snare", warehouse.resident)
        self.assertNotIn("a-new-kick", warehouse.resident)
        self.assertNotIn("z-new-snare", warehouse.resident)

    def test_unrelated_warehouse_content_may_be_evicted_but_old_kit_is_protected(self) -> None:
        warehouse = FakeWarehouse(20 * 1024)
        warehouse.resident = {
            "old-kick": 4 * 1024,
            "old-snare": 4 * 1024,
            "a-unrelated": 6 * 1024,
            "b-unrelated": 4 * 1024,
        }
        result = load_kit_transaction(
            old_pads={1: "old-kick", 2: "old-snare"},
            old_kit_id="old.v1",
            new_kit_id="new.v1",
            new_pad_assets={
                1: AssetPlan("new-kick", 5 * 1024),
                2: AssetPlan("new-snare", 5 * 1024),
            },
            warehouse=warehouse,
        )
        self.assertTrue(result.success)
        self.assertIn("old-kick", warehouse.resident)
        self.assertIn("old-snare", warehouse.resident)
        self.assertIn("new-kick", warehouse.resident)
        self.assertIn("new-snare", warehouse.resident)
        self.assertLessEqual(warehouse.used_bytes(), warehouse.capacity_bytes)

    def test_overlapping_old_and_new_assets_count_once_in_peak(self) -> None:
        warehouse = FakeWarehouse(24 * 1024)
        warehouse.resident = {"shared-kick": 8 * 1024, "old-snare": 6 * 1024}
        result = load_kit_transaction(
            old_pads={1: "shared-kick", 2: "old-snare"},
            old_kit_id="old.v1",
            new_kit_id="new.v1",
            new_pad_assets={
                1: AssetPlan("shared-kick", 8 * 1024),
                2: AssetPlan("new-snare", 6 * 1024),
            },
            warehouse=warehouse,
        )
        self.assertTrue(result.success)
        self.assertEqual(result.required_peak_bytes, 20 * 1024)

    def test_duplicate_asset_mapped_to_two_new_pads_is_loaded_once(self) -> None:
        warehouse = FakeWarehouse(16 * 1024)
        result = load_kit_transaction(
            old_pads={},
            old_kit_id="empty.v1",
            new_kit_id="shared.v1",
            new_pad_assets={
                1: AssetPlan("shared-hit", 8 * 1024),
                2: AssetPlan("shared-hit", 8 * 1024),
            },
            warehouse=warehouse,
        )
        self.assertTrue(result.success)
        self.assertEqual(result.required_peak_bytes, 8 * 1024)
        self.assertEqual(warehouse.resident, {"shared-hit": 8 * 1024})
        self.assertEqual(result.pads, {1: "shared-hit", 2: "shared-hit"})

    def test_slot_admission_fails_before_mutating_old_pads(self) -> None:
        warehouse = FakeWarehouse(64 * 1024, max_slots=2)
        warehouse.resident = {"old": 1024}
        result = load_kit_transaction(
            old_pads={1: "old"},
            old_kit_id="old.v1",
            new_kit_id="new.v1",
            new_pad_assets={
                1: AssetPlan("new-a", 1024),
                2: AssetPlan("new-b", 1024),
            },
            warehouse=warehouse,
        )
        self.assertFalse(result.success)
        self.assertEqual(result.pads, {1: "old"})
        self.assertEqual(result.scene_dirty_count, 0)
        self.assertEqual(warehouse.resident, {"old": 1024})


if __name__ == "__main__":
    unittest.main(verbosity=2)
