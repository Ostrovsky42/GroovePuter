#!/usr/bin/env python3
"""Compile and validate the deterministic HGS source/provenance manifest."""

import argparse
import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
LOCK_PATH = ROOT / "tools/harmony/source_lock.json"
QUALITY_REGISTRY_PATH = ROOT / "tools/harmony/quality_registry.json"
SCALE_REGISTRY_PATH = ROOT / "tools/harmony/scale_registry.json"
SCHEMA_VERSION = 1
_PITCH_MASK = (1 << 12) - 1


def _canonical(value):
    return json.dumps(value, ensure_ascii=False, sort_keys=True,
                      separators=(",", ":")) + "\n"


def _digest(value):
    return hashlib.sha256(_canonical(value).encode("utf-8")).hexdigest()


def _valid_int(value, low, high):
    return isinstance(value, int) and not isinstance(value, bool) and low <= value <= high


def _registry_index(rows, kind):
    if not isinstance(rows, list) or not rows:
        raise ValueError(f"{kind} registry must be a nonempty list")
    if any(not isinstance(row, dict) or
           not _valid_int(row.get("id"), 0, len(rows) - 1) for row in rows):
        raise ValueError(f"{kind} registry IDs must be integer values")
    if [row.get("id") for row in rows] != list(range(len(rows))):
        raise ValueError(f"{kind} registry IDs must be contiguous from zero")
    by_id = {}
    formulas = set()
    for row in rows:
        formula = row.get("intervals")
        if (not isinstance(formula, list) or not formula or
                any(not _valid_int(value, 0, 11) for value in formula) or
                formula != sorted(set(formula))):
            raise ValueError(f"{kind} {row.get('id')} has invalid intervals")
        mask = sum(1 << value for value in formula)
        if "interval_mask" in row and row["interval_mask"] != mask or \
                "cardinality" in row and row["cardinality"] != len(formula):
            raise ValueError(f"{kind} {row.get('id')} mask/cardinality mismatch")
        canonical = tuple(formula)
        if canonical in formulas:
            raise ValueError(f"{kind} registry contains a duplicate formula")
        formulas.add(canonical)
        by_id[row["id"]] = row
    return by_id


def _validate_vocab_rows(rows, registry, kind):
    if not isinstance(rows, list):
        raise ValueError(f"{kind} vocabulary must be a list")
    seen_ids = set()
    for row in rows:
        stable = registry.get(row.get("id"))
        if stable is None or row.get("id") in seen_ids:
            raise ValueError(f"{kind} vocabulary references an invalid/duplicate ID")
        seen_ids.add(row["id"])
        if row.get("intervals") != stable["intervals"]:
            raise ValueError(f"{kind} vocabulary formula disagrees with registry ID")
        aliases = row.get("aliases")
        if (not isinstance(aliases, list) or
                any(not isinstance(alias, str) for alias in aliases) or
                aliases != sorted(set(aliases))):
            raise ValueError(f"{kind} aliases must be sorted and unique")
        sources = row.get("sources")
        if not isinstance(sources, list) or not sources:
            raise ValueError(f"{kind} vocabulary row is missing source provenance")
        if any(not isinstance(source, dict) or
               not isinstance(source.get("path"), str) or
               not _valid_int(source.get("line"), 1, 1_000_000) for source in sources):
            raise ValueError(f"{kind} vocabulary has an invalid source locator")


def _validate_notice(notice):
    if not isinstance(notice, dict):
        raise ValueError("source license notice is missing")
    included = notice.get("included_path")
    if not isinstance(included, str):
        raise ValueError("source license notice path is missing")
    path = ROOT / included
    expected = notice.get("included_sha256", notice.get("repository_sha256"))
    actual = hashlib.sha256(path.read_bytes()).hexdigest()
    if actual != expected:
        raise ValueError(f"distributed license notice checksum mismatch: {included}")


def _validate_source_inputs(vocab, progression, vocab_manifest,
                            progression_manifest, lock):
    fannon = lock["sources"]["fannon"]
    ldrolez = lock["sources"]["ldrolez"]
    mingus = lock["sources"]["mingus_resolver"]
    if vocab.get("schema_version") != 1 or vocab_manifest.get("schema_version") != 1:
        raise ValueError("unsupported Fannon normalized vocabulary schema")
    if vocab.get("source_commit") != fannon["commit"] or \
            vocab_manifest.get("source_commit") != fannon["commit"]:
        raise ValueError("Fannon source commit mismatch")
    if vocab_manifest.get("source_file_sha256") != fannon["files"]:
        raise ValueError("Fannon source file provenance mismatch")
    if vocab_manifest.get("quality_count") != len(vocab.get("qualities", [])) or \
            vocab_manifest.get("scale_count") != len(vocab.get("scales", [])):
        raise ValueError("Fannon vocabulary manifest cardinality mismatch")
    if vocab_manifest.get("normalized_content_sha256") != _digest(vocab):
        raise ValueError("Fannon normalized content checksum mismatch")

    if progression.get("schema_version") != 1 or \
            progression_manifest.get("schema_version") != 1:
        raise ValueError("unsupported progression import schema")
    if progression.get("sources", {}).get("ldrolez_commit") != ldrolez["commit"] or \
            progression_manifest.get("ldrolez_commit") != ldrolez["commit"]:
        raise ValueError("ldrolez source commit mismatch")
    if progression_manifest.get("ldrolez_files") != ldrolez["files"]:
        raise ValueError("ldrolez source file provenance mismatch")
    if progression.get("sources", {}).get("mingus_resolver_commit") != mingus["commit"] or \
            progression_manifest.get("mingus_resolver_commit") != mingus["commit"]:
        raise ValueError("Mingus resolver commit mismatch")
    if progression_manifest.get("mingus_resolver_files") != mingus["files"]:
        raise ValueError("Mingus resolver provenance mismatch")
    accepted = progression.get("accepted")
    quarantined = progression.get("quarantine")
    if not isinstance(accepted, list) or not isinstance(quarantined, list):
        raise ValueError("progression input must contain accepted and quarantine lists")
    counts = (len(accepted), len(quarantined), len(accepted) + len(quarantined))
    if (progression_manifest.get("accepted_count"),
            progression_manifest.get("quarantined_count"),
            progression_manifest.get("row_count")) != counts:
        raise ValueError("progression import manifest row counts mismatch")
    if progression_manifest.get("content_sha256") != _digest(progression):
        raise ValueError("progression normalized content checksum mismatch")
    for row in accepted:
        if row.get("status") != "accepted" or not row.get("source", {}).get("line"):
            raise ValueError("accepted route missing status or source locator")
    for row in quarantined:
        if row.get("status") != "quarantined" or not row.get("source", {}).get("line"):
            raise ValueError("quarantined route missing status or source locator")
    _validate_notice(fannon["license"])
    _validate_notice(fannon["tonal_license"])
    _validate_notice(ldrolez["license"])
    _validate_notice(mingus["license"])


def _content_projection(model):
    """Return source-path-independent, deterministic model content."""
    def vocabulary_rows(rows):
        return [{key: row[key] for key in (
            "id", "name", "intervals", "interval_mask", "cardinality",
            "family", "aliases", "skeleton", "complexity", "mode", "parent"
        ) if key in row} for row in rows]

    routes = []
    for route in model["corpus"]["routes"]:
        routes.append({
            "id": route["id"],
            "tokens": route["tokens"],
            "mood_tags": route["mood_tags"],
            "weight": route["weight"],
        })
    return {
        "schema_version": model["schema_version"],
        "quality_registry": model["quality_registry"],
        "scale_registry": model["scale_registry"],
        "quality_vocabulary": vocabulary_rows(model["quality_vocabulary"]),
        "scale_vocabulary": vocabulary_rows(model["scale_vocabulary"]),
        "corpus_routes": routes,
        "quarantine_counts": model["corpus"]["quarantine_counts"],
    }


def _route_key(row, quality_ids):
    canonical = []
    for token in row["route"]:
        quality_id = token.get("quality_id")
        if quality_id not in quality_ids:
            raise ValueError(f"route references unknown quality ID {quality_id!r}")
        if not _valid_int(token.get("degree"), 1, 7):
            raise ValueError("route token degree must be 1..7")
        if not _valid_int(token.get("accidental"), -12, 12):
            raise ValueError("route token accidental must be -12..12")
        if not _valid_int(token.get("root_offset"), 0, 11):
            raise ValueError("route token root offset must be 0..11")
        slash_root = token.get("slash_bass_offset")
        slash_interval = token.get("slash_bass_interval")
        if slash_root is not None:
            if not _valid_int(slash_root, 0, 11) or \
                    not _valid_int(slash_interval, 0, 11):
                raise ValueError("slash bass must carry a valid root and interval")
            if slash_interval != (slash_root - token["root_offset"]) % 12:
                raise ValueError("slash bass interval disagrees with route roots")
        elif slash_interval is not None:
            raise ValueError("slash bass interval supplied without slash bass root")
        suffix = token.get("suffix")
        if not isinstance(suffix, str):
            raise ValueError("route token suffix must be text")
        canonical.append({
            "degree": token["degree"],
            "accidental": token["accidental"],
            "root_offset": token["root_offset"],
            "quality_id": quality_id,
            "suffix": suffix,
            "slash_bass_offset": slash_root,
            "slash_bass_interval": slash_interval,
        })
    if not canonical:
        raise ValueError("accepted progression has an empty route")
    return canonical


def build_model(vocab, progression, vocab_manifest, progression_manifest,
                lock, quality_registry, scale_registry):
    """Build a schema-v1 normalized corpus manifest from pinned import outputs."""
    _validate_source_inputs(vocab, progression, vocab_manifest,
                            progression_manifest, lock)
    quality_ids = _registry_index(quality_registry["qualities"], "quality")
    scale_ids = _registry_index(scale_registry["scales"], "scale")
    _validate_vocab_rows(vocab["qualities"], quality_ids, "quality")
    _validate_vocab_rows(vocab["scales"], scale_ids, "scale")

    grouped = {}
    for row in progression["accepted"]:
        tokens = _route_key(row, quality_ids)
        key = _canonical(tokens)
        group = grouped.setdefault(key, {
            "tokens": tokens, "mood_tags": set(), "source_rows": [],
        })
        group["mood_tags"].update(row.get("mood_tags", []))
        group["source_rows"].append({
            "path": row["source"]["path"],
            "line": row["source"]["line"],
            "list": row["source"].get("list", ""),
            "index": row["source"].get("index", 0),
            "raw": row["raw"],
            "route_text": row["route_text"],
            "source_tokens": row["source_tokens"],
            "source_dialect": row["source_dialect"],
            "context": row["context"],
        })

    routes = []
    for key, group in sorted(grouped.items()):
        route_id = "r_" + hashlib.sha256(key.encode("utf-8")).hexdigest()[:16]
        sources = sorted(group["source_rows"], key=lambda source: (
            source["path"], source["line"], source["list"], source["index"]))
        routes.append({
            "id": route_id,
            "tokens": group["tokens"],
            "mood_tags": sorted(group["mood_tags"]),
            "weight": 1,
            "source_locators": sources,
        })
    if len({route["id"] for route in routes}) != len(routes):
        raise ValueError("route ID hash collision")

    quality_vocabulary = sorted(vocab["qualities"], key=lambda row: row["id"])
    scale_vocabulary = sorted(vocab["scales"], key=lambda row: row["id"])

    quarantine_counts = {}
    for row in progression["quarantine"]:
        reason = row.get("reason_code", "unspecified")
        quarantine_counts[reason] = quarantine_counts.get(reason, 0) + 1
    def enrich_registry(rows):
        enriched = []
        for row in rows:
            item = dict(row)
            item["interval_mask"] = sum(1 << pitch for pitch in row["intervals"])
            item["cardinality"] = len(row["intervals"])
            enriched.append(item)
        return enriched

    model = {
        "schema_version": SCHEMA_VERSION,
        "quality_registry": enrich_registry(quality_registry["qualities"]),
        "scale_registry": enrich_registry(scale_registry["scales"]),
        "quality_vocabulary": quality_vocabulary,
        "scale_vocabulary": scale_vocabulary,
        "corpus": {
            "unit": "unique_normalized_route",
            "raw_accepted_count": len(progression["accepted"]),
            "unique_route_count": len(routes),
            "duplicate_route_count": len(progression["accepted"]) - len(routes),
            "quarantined_count": len(progression["quarantine"]),
            "quarantine_counts": dict(sorted(quarantine_counts.items())),
            "routes": routes,
        },
        "provenance": {
            "source_lock_sha256": hashlib.sha256(LOCK_PATH.read_bytes()).hexdigest(),
            "sources": {
                "fannon": {
                    "repository": lock["sources"]["fannon"]["repository"],
                    "commit": lock["sources"]["fannon"]["commit"],
                    "files": lock["sources"]["fannon"]["files"],
                    "role": "vocabulary_only",
                    "license": lock["sources"]["fannon"]["license"],
                    "tonal_license": lock["sources"]["fannon"]["tonal_license"],
                },
                "ldrolez": {
                    "repository": lock["sources"]["ldrolez"]["repository"],
                    "commit": lock["sources"]["ldrolez"]["commit"],
                    "files": lock["sources"]["ldrolez"]["files"],
                    "role": "progression_corpus",
                    "license": lock["sources"]["ldrolez"]["license"],
                },
                "mingus_resolver": {
                    "repository": lock["sources"]["mingus_resolver"]["repository"],
                    "tag": lock["sources"]["mingus_resolver"]["tag"],
                    "commit": lock["sources"]["mingus_resolver"]["commit"],
                    "files": lock["sources"]["mingus_resolver"]["files"],
                    "role": "notation_semantics_only",
                    "license": lock["sources"]["mingus_resolver"]["license"],
                },
            },
        },
    }
    model["content_sha256"] = _digest(_content_projection(model))
    validate_model(model)
    return model


def validate_model(model):
    """Reject schema, stable-ID, formula, provenance and content-checksum drift."""
    if model.get("schema_version") != SCHEMA_VERSION:
        raise ValueError("unsupported harmony model schema")
    quality_ids = _registry_index(model.get("quality_registry"), "quality")
    _registry_index(model.get("scale_registry"), "scale")
    for kind, rows in (("quality", model["quality_registry"]),
                       ("scale", model["scale_registry"])):
        if any("interval_mask" not in row or "cardinality" not in row for row in rows):
            raise ValueError(f"{kind} registry is missing formula mask/cardinality")
    _validate_vocab_rows(model.get("quality_vocabulary"), quality_ids, "quality")
    _validate_vocab_rows(model.get("scale_vocabulary"),
                         _registry_index(model["scale_registry"], "scale"), "scale")
    corpus = model.get("corpus")
    if not isinstance(corpus, dict) or corpus.get("unit") != "unique_normalized_route":
        raise ValueError("corpus unit must be unique_normalized_route")
    routes = corpus.get("routes")
    if not isinstance(routes, list) or \
            corpus.get("unique_route_count") != len(routes) or \
            corpus.get("raw_accepted_count") != len(routes) + \
            corpus.get("duplicate_route_count"):
        raise ValueError("corpus route counts mismatch")
    if corpus.get("quarantined_count") != sum(corpus.get("quarantine_counts", {}).values()):
        raise ValueError("quarantine count summary mismatch")
    route_ids = set()
    for route in routes:
        if route.get("id") in route_ids:
            raise ValueError("duplicate canonical route ID")
        route_ids.add(route.get("id"))
        tokens = _route_key({"route": route.get("tokens", [])}, set(quality_ids))
        expected_id = "r_" + hashlib.sha256(_canonical(tokens).encode("utf-8")).hexdigest()[:16]
        if route.get("id") != expected_id:
            raise ValueError("canonical route ID does not match its token content")
        if route.get("weight") != 1:
            raise ValueError("normalized route statistical weight must be one")
        if route.get("mood_tags") != sorted(set(route.get("mood_tags", []))):
            raise ValueError("route mood tags must be sorted and unique")
        sources = route.get("source_locators")
        if not isinstance(sources, list) or not sources:
            raise ValueError("canonical route is missing source locators")
        if any(not isinstance(source, dict) or
               not isinstance(source.get("path"), str) or
               not _valid_int(source.get("line"), 1, 1_000_000)
               for source in sources):
            raise ValueError("canonical route has an invalid source locator")
    provenance = model.get("provenance", {})
    sources = provenance.get("sources", {})
    if set(sources) != {"fannon", "ldrolez", "mingus_resolver"}:
        raise ValueError("manifest source provenance is incomplete")
    lock = json.loads(LOCK_PATH.read_text(encoding="utf-8"))
    expected_lock_hash = hashlib.sha256(LOCK_PATH.read_bytes()).hexdigest()
    if provenance.get("source_lock_sha256") != expected_lock_hash:
        raise ValueError("source lock checksum mismatch")
    for name, source in sources.items():
        locked = lock["sources"][name]
        if source.get("repository") != locked["repository"] or \
                source.get("commit") != locked["commit"] or \
                source.get("files") != locked["files"]:
            raise ValueError(f"{name} provenance disagrees with source lock")
        if source.get("license") != locked["license"]:
            raise ValueError(f"{name} license provenance disagrees with source lock")
    if sources["mingus_resolver"].get("tag") != lock["sources"]["mingus_resolver"]["tag"] or \
            sources["fannon"].get("tonal_license") != lock["sources"]["fannon"]["tonal_license"]:
        raise ValueError("resolver tag or Tonal.js license provenance mismatch")
    if sources["mingus_resolver"].get("role") != "notation_semantics_only":
        raise ValueError("Mingus source role is invalid")
    if sources["fannon"].get("role") != "vocabulary_only" or \
            sources["ldrolez"].get("role") != "progression_corpus":
        raise ValueError("source role allows vocabulary to contaminate route statistics")
    for source in sources.values():
        _validate_notice(source.get("license"))
    _validate_notice(sources["fannon"].get("tonal_license"))
    expected = _digest(_content_projection(model))
    if model.get("content_sha256") != expected:
        raise ValueError("harmony model content checksum mismatch")
    return True


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--vocab", required=True, type=Path)
    parser.add_argument("--vocab-manifest", required=True, type=Path)
    parser.add_argument("--progressions", required=True, type=Path)
    parser.add_argument("--progressions-manifest", required=True, type=Path)
    parser.add_argument("--out-dir", required=True, type=Path)
    args = parser.parse_args()
    lock = json.loads(LOCK_PATH.read_text(encoding="utf-8"))
    quality_registry = json.loads(QUALITY_REGISTRY_PATH.read_text(encoding="utf-8"))
    scale_registry = json.loads(SCALE_REGISTRY_PATH.read_text(encoding="utf-8"))
    model = build_model(
        json.loads(args.vocab.read_text(encoding="utf-8")),
        json.loads(args.progressions.read_text(encoding="utf-8")),
        json.loads(args.vocab_manifest.read_text(encoding="utf-8")),
        json.loads(args.progressions_manifest.read_text(encoding="utf-8")),
        lock, quality_registry, scale_registry,
    )
    args.out_dir.mkdir(parents=True, exist_ok=True)
    (args.out_dir / "harmony_manifest.json").write_text(
        _canonical(model), encoding="utf-8")
    print(f"schema={SCHEMA_VERSION} routes={model['corpus']['unique_route_count']} "
          f"quarantine={model['corpus']['quarantined_count']} "
          f"content_sha256={model['content_sha256']}")


if __name__ == "__main__":
    main()
