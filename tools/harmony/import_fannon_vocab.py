#!/usr/bin/env python3
"""Normalize pinned Fannon vocabulary into stable offline HGS records."""

import argparse
import hashlib
import json
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
LOCK_PATH = ROOT / "tools/harmony/source_lock.json"
QUALITY_REGISTRY_PATH = ROOT / "tools/harmony/quality_registry.json"
SCALE_REGISTRY_PATH = ROOT / "tools/harmony/scale_registry.json"


def _pc_list(record):
    root = record.get("root_pc")
    values = record.get("pitch_classes")
    if isinstance(root, bool) or not isinstance(root, int) or not 0 <= root < 12:
        raise ValueError("root_pc must be an integer in 0..11")
    if not isinstance(values, list) or not values:
        raise ValueError("pitch_classes must be a nonempty list")
    if any(isinstance(value, bool) or not isinstance(value, int) or
           not 0 <= value < 12 for value in values):
        raise ValueError("pitch_classes must contain integers in 0..11")
    return sorted({(value - root) % 12 for value in values})


def _mask(intervals):
    return sum(1 << interval for interval in intervals)


def normalize_quality(record):
    """Return root-independent formula fields while retaining source identity."""
    intervals = _pc_list(record)
    return {
        "symbol": str(record.get("symbol", "")),
        "export_name": str(record.get("export_name", "")),
        "intervals": intervals,
        "interval_mask": _mask(intervals),
        "cardinality": len(intervals),
        "tonal_aliases": sorted(set(record.get("tonal_aliases", []))),
        "source": dict(record.get("source", {})),
    }


def normalize_scale(record):
    """Return root-independent scale fields while retaining source identity."""
    intervals = _pc_list(record)
    return {
        "symbol": str(record.get("symbol", "")),
        "export_name": str(record.get("export_name", "")),
        "intervals": intervals,
        "interval_mask": _mask(intervals),
        "cardinality": len(intervals),
        "tonal_aliases": sorted(set(record.get("tonal_aliases", []))),
        "source": dict(record.get("source", {})),
    }


def registry_index(rows):
    """Validate stable append-only IDs and index exact formula tuples."""
    if [row.get("id") for row in rows] != list(range(len(rows))):
        raise ValueError("registry IDs must be contiguous and append-only from zero")
    result = {}
    for row in rows:
        intervals = tuple(row.get("intervals", []))
        if not intervals or tuple(sorted(set(intervals))) != intervals:
            raise ValueError("registry intervals must be sorted unique nonempty pitch classes")
        if any(isinstance(value, bool) or not isinstance(value, int) or
               not 0 <= value < 12 for value in intervals):
            raise ValueError("registry intervals must be in 0..11")
        if intervals in result:
            raise ValueError("duplicate formula in stable registry")
        result[intervals] = row
    return result


def _merge_records(records, normalizer, registry):
    grouped = {}
    quarantine = []
    for record in records:
        normalized = normalizer(record)
        formula = tuple(normalized["intervals"])
        stable = registry.get(formula)
        if stable is None:
            quarantine.append({
                "kind": "quality" if normalizer is normalize_quality else "scale",
                "symbol": normalized["symbol"],
                "intervals": list(formula),
                "reason": "formula has no explicit stable registry ID",
                "source": normalized["source"],
            })
            continue
        row = grouped.setdefault(formula, {
            "id": stable["id"],
            "name": stable["name"],
            "intervals": list(formula),
            "interval_mask": _mask(formula),
            "cardinality": len(formula),
            "family": stable["family"],
            "sources": [],
        })
        for metadata_key in ("skeleton", "complexity", "mode", "parent"):
            if metadata_key in stable:
                row[metadata_key] = stable[metadata_key]
        aliases = set(row.get("aliases", []))
        for alias in [normalized["symbol"], normalized["export_name"],
                      *normalized["tonal_aliases"]]:
            if alias:
                aliases.add(alias)
        row["aliases"] = sorted(aliases)
        source = {
            **normalized["source"],
            "symbol": normalized["symbol"],
            "export_name": normalized["export_name"],
        }
        if source not in row["sources"]:
            row["sources"].append(source)

    rows = list(grouped.values())
    for row in rows:
        row["sources"].sort(key=lambda value: (
            value.get("path", ""), value.get("line", 0),
            value.get("symbol", ""), value.get("export_variant", "")))
    rows.sort(key=lambda value: value["id"])
    quarantine.sort(key=lambda value: (
        value["kind"], value["intervals"], value["symbol"],
        value["source"].get("path", ""), value["source"].get("line", 0)))
    return rows, quarantine


def compile_vocabulary(exported, quality_registry, scale_registry):
    quality_index = registry_index(quality_registry["qualities"])
    scale_index = registry_index(scale_registry["scales"])
    qualities, quality_quarantine = _merge_records(
        exported.get("qualities", []), normalize_quality, quality_index)
    scales, scale_quarantine = _merge_records(
        exported.get("scales", []), normalize_scale, scale_index)
    return {
        "schema_version": 1,
        "source_commit": exported.get("source_commit", ""),
        "tonal_packages": exported.get("tonal_packages", {}),
        "qualities": qualities,
        "scales": scales,
        "quarantine": quality_quarantine + scale_quarantine,
    }


def _sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def verify_source(source_root, exported, lock):
    source = lock["sources"]["fannon"]
    actual_commit = subprocess.check_output(
        ["git", "-C", str(source_root), "rev-parse", "HEAD"], text=True
    ).strip()
    if actual_commit != source["commit"]:
        raise ValueError(f"Fannon source commit mismatch: {actual_commit}")
    expected_hashes = source["files"]
    for relative_path, expected in expected_hashes.items():
        actual = _sha256(source_root / relative_path)
        if actual != expected:
            raise ValueError(f"Fannon source digest mismatch: {relative_path}")
    license_actual = _sha256(source_root / source["license"]["source_path"])
    if license_actual != source["license"]["repository_sha256"]:
        raise ValueError("Fannon license digest mismatch")
    if exported.get("source_commit") != source["commit"]:
        raise ValueError("export does not identify the pinned Fannon source commit")
    if exported.get("source_file_sha256") != expected_hashes:
        raise ValueError("export source digests differ from source lock")
    expected_counts = source["expected_input_counts"]
    for name, expected in expected_counts.items():
        if len(exported.get(name, [])) != expected:
            raise ValueError(f"Fannon {name} count mismatch")
    if exported.get("tonal_packages") != source["tonal_packages"]:
        raise ValueError("resolved Tonal.js versions/licenses differ from source lock")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--export-json", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()

    lock = json.loads(LOCK_PATH.read_text(encoding="utf-8"))
    exported = json.loads(args.export_json.read_text(encoding="utf-8"))
    verify_source(args.source.resolve(), exported, lock)
    quality_registry = json.loads(QUALITY_REGISTRY_PATH.read_text(encoding="utf-8"))
    scale_registry = json.loads(SCALE_REGISTRY_PATH.read_text(encoding="utf-8"))
    compiled = compile_vocabulary(exported, quality_registry, scale_registry)
    content = json.dumps(compiled, ensure_ascii=False, sort_keys=True,
                         separators=(",", ":")) + "\n"
    content_hash = hashlib.sha256(content.encode("utf-8")).hexdigest()
    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / "fannon_vocab.json").write_text(content, encoding="utf-8")
    manifest = {
        "schema_version": 1,
        "source_commit": lock["sources"]["fannon"]["commit"],
        "source_file_sha256": lock["sources"]["fannon"]["files"],
        "normalized_content_sha256": content_hash,
        "quality_count": len(compiled["qualities"]),
        "scale_count": len(compiled["scales"]),
        "quarantined_count": len(compiled["quarantine"]),
    }
    (args.out / "manifest.json").write_text(
        json.dumps(manifest, sort_keys=True, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
