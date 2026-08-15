#!/usr/bin/env python3
import json
import unittest

from test_sampler_0_9_5_kit_contract import (
    KitContractError,
    MAX_ASSET_LOCATOR_BYTES,
    validate_manifest,
)

MAX_KIT_MANIFEST_BYTES = 1536


class ManifestWireError(ValueError):
    pass


def _reject_constant(value: str):
    raise ManifestWireError(f"non-finite JSON constant rejected: {value}")


def _strict_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ManifestWireError(f"duplicate JSON key rejected: {key}")
        result[key] = value
    return result


def parse_manifest_wire(raw: bytes):
    if not isinstance(raw, bytes):
        raise ManifestWireError("manifest wire input must be bytes")
    if not raw or len(raw) > MAX_KIT_MANIFEST_BYTES:
        raise ManifestWireError("manifest byte size out of range")
    if raw.startswith(b"\xef\xbb\xbf"):
        raise ManifestWireError("UTF-8 BOM rejected")

    try:
        text = raw.decode("utf-8", errors="strict")
    except UnicodeDecodeError as exc:
        raise ManifestWireError("manifest must be valid UTF-8") from exc

    try:
        document = json.loads(
            text,
            object_pairs_hook=_strict_object,
            parse_constant=_reject_constant,
        )
    except (json.JSONDecodeError, ManifestWireError) as exc:
        raise ManifestWireError("invalid strict JSON manifest") from exc

    try:
        return validate_manifest(document)
    except KitContractError as exc:
        raise ManifestWireError(str(exc)) from exc


def canonical_manifest_bytes() -> bytes:
    document = {
        "schema": 1,
        "id": "sp12.factory.v1",
        "name": "SP12",
        "pads": [
            {"pad": 1, "file": "kick.wav"},
            {"pad": 2, "file": "snare.wav"},
            {"pad": 3, "file": "closed_hat.wav"},
            {"pad": 4, "file": "open_hat.wav"},
            {"pad": 8, "file": "clap.wav"},
        ],
    }
    return json.dumps(
        document,
        ensure_ascii=False,
        separators=(",", ":"),
        sort_keys=False,
    ).encode("utf-8")


class ManifestWireContractTests(unittest.TestCase):
    def test_canonical_manifest_parses_from_bounded_utf8_json(self) -> None:
        raw = canonical_manifest_bytes()
        self.assertLessEqual(len(raw), MAX_KIT_MANIFEST_BYTES)
        manifest = parse_manifest_wire(raw)
        self.assertEqual(manifest.schema, 1)
        self.assertEqual(manifest.kit_id, "sp12.factory.v1")
        self.assertEqual(manifest.name, "SP12")
        self.assertEqual([asset.pad for asset in manifest.assets], [1, 2, 3, 4, 8])

    def test_pretty_printed_manifest_is_still_valid(self) -> None:
        document = json.loads(canonical_manifest_bytes())
        raw = json.dumps(document, indent=2, ensure_ascii=False).encode("utf-8")
        self.assertLessEqual(len(raw), MAX_KIT_MANIFEST_BYTES)
        self.assertEqual(parse_manifest_wire(raw).kit_id, "sp12.factory.v1")

    def test_manifest_cap_accommodates_eight_max_length_asset_locators(self) -> None:
        prefix = "drums/"
        suffix = ".wav"
        stem_len = MAX_ASSET_LOCATOR_BYTES - len(prefix) - len(suffix)
        locator = prefix + ("a" * stem_len) + suffix
        self.assertEqual(len(locator.encode("utf-8")), MAX_ASSET_LOCATOR_BYTES)

        document = {
            "schema": 1,
            "id": "x" * 64,
            "name": "N" * 32,
            "pads": [
                {"pad": pad, "file": locator}
                for pad in range(1, 9)
            ],
        }
        raw = json.dumps(document, separators=(",", ":")).encode("utf-8")
        self.assertLessEqual(len(raw), MAX_KIT_MANIFEST_BYTES)
        manifest = parse_manifest_wire(raw)
        self.assertEqual(len(manifest.assets), 8)

    def test_oversized_manifest_rejected_before_json_parse(self) -> None:
        raw = b"{" + (b" " * MAX_KIT_MANIFEST_BYTES) + b"}"
        with self.assertRaises(ManifestWireError):
            parse_manifest_wire(raw)

    def test_utf8_bom_rejected(self) -> None:
        with self.assertRaises(ManifestWireError):
            parse_manifest_wire(b"\xef\xbb\xbf" + canonical_manifest_bytes())

    def test_invalid_utf8_rejected(self) -> None:
        with self.assertRaises(ManifestWireError):
            parse_manifest_wire(canonical_manifest_bytes() + b"\xff")

    def test_duplicate_top_level_key_rejected(self) -> None:
        raw = (
            b'{"schema":1,"id":"kit.v1","id":"shadow.v1",'
            b'"name":"Kit","pads":[{"pad":1,"file":"kick.wav"}]}'
        )
        with self.assertRaises(ManifestWireError):
            parse_manifest_wire(raw)

    def test_duplicate_pad_entry_key_rejected(self) -> None:
        raw = (
            b'{"schema":1,"id":"kit.v1","name":"Kit",'
            b'"pads":[{"pad":1,"pad":2,"file":"kick.wav"}]}'
        )
        with self.assertRaises(ManifestWireError):
            parse_manifest_wire(raw)

    def test_trailing_json_garbage_rejected(self) -> None:
        with self.assertRaises(ManifestWireError):
            parse_manifest_wire(canonical_manifest_bytes() + b" trailing")

    def test_nonfinite_json_numbers_rejected(self) -> None:
        raw = (
            b'{"schema":NaN,"id":"kit.v1","name":"Kit",'
            b'"pads":[{"pad":1,"file":"kick.wav"}]}'
        )
        with self.assertRaises(ManifestWireError):
            parse_manifest_wire(raw)

    def test_unknown_top_level_fields_rejected_by_schema_v1(self) -> None:
        document = json.loads(canonical_manifest_bytes())
        document["future"] = True
        raw = json.dumps(document, separators=(",", ":")).encode("utf-8")
        with self.assertRaises(ManifestWireError):
            parse_manifest_wire(raw)


if __name__ == "__main__":
    unittest.main(verbosity=2)
