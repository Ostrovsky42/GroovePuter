#!/usr/bin/env python3
from dataclasses import dataclass
import unittest

from test_sampler_0_9_5_kit_contract import KIT_ID_RE, validate_asset_locator
from test_sampler_0_9_5_missing_relink_contract import validate_locator


FNV64_OFFSET = 14695981039346656037
FNV64_PRIME = 1099511628211


class IdentityContractError(ValueError):
    pass


def fnv1a64(value: str) -> int:
    h = FNV64_OFFSET
    for byte in value.encode("utf-8"):
        h ^= byte
        h = (h * FNV64_PRIME) & 0xFFFFFFFFFFFFFFFF
    return h or 1


def _valid_kit_id(kit_id: str) -> bool:
    return bool(KIT_ID_RE.fullmatch(kit_id))


def canonical_loose_key(path: str) -> str:
    normalized = path.replace("\\", "/")
    while "//" in normalized:
        normalized = normalized.replace("//", "/")
    normalized = normalized.lstrip("/")
    if normalized.startswith("sd/"):
        normalized = normalized[3:]
    while normalized.startswith("./"):
        normalized = normalized[2:]
    if not normalized.startswith("samples/"):
        raise IdentityContractError("loose sample must live below /samples")
    relative = normalized[len("samples/"):]
    try:
        validated = validate_asset_locator(relative)
    except ValueError as exc:
        raise IdentityContractError("invalid loose sample path") from exc
    return f"samples/{validated}"


def loose_sample_ref(path: str) -> int:
    return fnv1a64(canonical_loose_key(path))


def kit_asset_locator(kit_id: str, relative_asset: str) -> str:
    if not _valid_kit_id(kit_id):
        raise IdentityContractError("invalid stable kit id")
    try:
        validated_asset = validate_asset_locator(relative_asset)
    except ValueError as exc:
        raise IdentityContractError("invalid kit asset path") from exc
    locator = f"kit:{kit_id}:{validated_asset}"
    if not validate_locator(locator):
        raise IdentityContractError("logical locator violates persistence bound")
    return locator


def kit_asset_ref(kit_id: str, relative_asset: str) -> int:
    # Stable kit sample identity is deliberately derived from logical product
    # identity, not from a physical /kits/<directory>/ path.
    return fnv1a64(kit_asset_locator(kit_id, relative_asset))


@dataclass(frozen=True)
class KitCatalogEntry:
    kit_id: str
    physical_directory: str


class KitCatalogResolver:
    def __init__(self) -> None:
        self.entries: dict[str, KitCatalogEntry] = {}
        self.asset_paths: dict[int, str] = {}

    def register_kit(self, kit_id: str, physical_directory: str) -> None:
        if not _valid_kit_id(kit_id):
            raise IdentityContractError("invalid kit id")
        if not physical_directory.startswith("/kits/"):
            raise IdentityContractError("kit directory outside canonical root")
        if physical_directory.endswith("/"):
            physical_directory = physical_directory[:-1]
        existing = self.entries.get(kit_id)
        if existing is not None and existing.physical_directory != physical_directory:
            raise IdentityContractError("duplicate stable kit id")
        self.entries[kit_id] = KitCatalogEntry(kit_id, physical_directory)

    def bind_asset(self, kit_id: str, relative_asset: str) -> int:
        entry = self.entries.get(kit_id)
        if entry is None:
            raise IdentityContractError("unknown kit id")
        ref = kit_asset_ref(kit_id, relative_asset)
        physical = f"{entry.physical_directory}/{relative_asset}"
        existing = self.asset_paths.get(ref)
        if existing is not None and existing != physical:
            raise IdentityContractError("logical SampleRef ownership collision")
        self.asset_paths[ref] = physical
        return ref

    def physical_path(self, ref: int) -> str | None:
        return self.asset_paths.get(ref)


class LogicalIdentityContractTests(unittest.TestCase):
    def test_loose_identity_preserves_current_sd_mount_alias_semantics(self) -> None:
        refs = {
            loose_sample_ref("/samples/kick.wav"),
            loose_sample_ref("/sd/samples/kick.wav"),
            loose_sample_ref("samples/kick.wav"),
        }
        self.assertEqual(len(refs), 1)
        self.assertEqual(canonical_loose_key("/sd/samples/kick.wav"),
                         "samples/kick.wav")

    def test_kit_asset_identity_is_independent_from_directory_name(self) -> None:
        before = kit_asset_ref("sp12.factory.v1", "snare.wav")
        after = kit_asset_ref("sp12.factory.v1", "snare.wav")
        self.assertEqual(before, after)

        catalog_before = KitCatalogResolver()
        catalog_before.register_kit("sp12.factory.v1", "/kits/SP12")
        ref_before = catalog_before.bind_asset("sp12.factory.v1", "snare.wav")
        self.assertEqual(catalog_before.physical_path(ref_before),
                         "/kits/SP12/snare.wav")

        catalog_after = KitCatalogResolver()
        catalog_after.register_kit("sp12.factory.v1", "/kits/MyRenamedSP12")
        ref_after = catalog_after.bind_asset("sp12.factory.v1", "snare.wav")
        self.assertEqual(ref_before, ref_after)
        self.assertEqual(catalog_after.physical_path(ref_after),
                         "/kits/MyRenamedSP12/snare.wav")

    def test_same_filename_in_different_kits_has_distinct_identity(self) -> None:
        a = kit_asset_ref("sp12.factory.v1", "snare.wav")
        b = kit_asset_ref("909tape.factory.v1", "snare.wav")
        self.assertNotEqual(a, b)

    def test_different_asset_paths_in_same_kit_have_distinct_identity(self) -> None:
        a = kit_asset_ref("mykit.v1", "snare.wav")
        b = kit_asset_ref("mykit.v1", "alt/snare.wav")
        self.assertNotEqual(a, b)

    def test_logical_locator_and_sample_ref_are_exactly_aligned(self) -> None:
        locator = kit_asset_locator("sp12.factory.v1", "drums/snare.wav")
        self.assertEqual(locator, "kit:sp12.factory.v1:drums/snare.wav")
        self.assertEqual(kit_asset_ref("sp12.factory.v1", "drums/snare.wav"),
                         fnv1a64(locator))

    def test_duplicate_stable_kit_id_fails_closed(self) -> None:
        catalog = KitCatalogResolver()
        catalog.register_kit("sp12.factory.v1", "/kits/SP12")
        with self.assertRaises(IdentityContractError):
            catalog.register_kit("sp12.factory.v1", "/kits/OtherSP12")

    def test_invalid_asset_cannot_escape_kit_root(self) -> None:
        for bad in ("../scene.json", "/snare.wav", "drums\\snare.wav", "./snare.wav"):
            with self.assertRaises(IdentityContractError):
                kit_asset_ref("sp12.factory.v1", bad)

    def test_loose_and_kit_namespaces_do_not_alias(self) -> None:
        loose = loose_sample_ref("/samples/sp12.factory.v1/snare.wav")
        kit = kit_asset_ref("sp12.factory.v1", "snare.wav")
        self.assertNotEqual(loose, kit)

    def test_physical_catalog_binding_does_not_redefine_identity(self) -> None:
        catalog = KitCatalogResolver()
        catalog.register_kit("mykit.v1", "/kits/MyKit")
        ref = catalog.bind_asset("mykit.v1", "kick.wav")
        self.assertEqual(ref, fnv1a64("kit:mykit.v1:kick.wav"))
        self.assertNotEqual(ref, fnv1a64("kits/MyKit/kick.wav"))


if __name__ == "__main__":
    unittest.main(verbosity=2)
