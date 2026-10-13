#!/usr/bin/env python3
"""Compile deterministic, transposition-invariant transition counts."""

import argparse
import hashlib
import json
import struct
from collections import Counter, defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
TOKEN_SCHEMA = "canonical-function-v1"
TOKEN_SIZE = 8
QUALITY_GROUPS = {
    "major": 0,
    "minor": 1,
    "dominant": 2,
    "diminished": 3,
    "augmented": 4,
    "sus4": 5,
    "sus2": 6,
    "power": 7,
    "other": 8,
}


def _mask(values):
    return sum(1 << value for value in values)


def _quality_group(quality):
    family = quality["family"]
    skeleton = tuple(quality["skeleton"])
    if family == "MajorMinor":
        if skeleton == (0, 4, 7):
            return QUALITY_GROUPS["major"]
        if skeleton == (0, 3, 7):
            return QUALITY_GROUPS["minor"]
        return QUALITY_GROUPS["other"]
    if family == "Dominant":
        return QUALITY_GROUPS["dominant"]
    if family == "Diminished":
        return QUALITY_GROUPS["diminished"]
    if family == "Augmented":
        return QUALITY_GROUPS["augmented"]
    if family == "Suspended":
        if skeleton == (0, 5, 7):
            return QUALITY_GROUPS["sus4"]
        if skeleton == (0, 2, 7):
            return QUALITY_GROUPS["sus2"]
        if skeleton == (0, 7):
            return QUALITY_GROUPS["power"]
    return QUALITY_GROUPS["other"]


def _canonical_token(raw, quality_by_id):
    if not isinstance(raw, dict):
        raise ValueError("route token must be an object")
    degree = raw.get("degree")
    root_offset = raw.get("root_offset")
    quality_id = raw.get("quality_id")
    accidental = raw.get("accidental", 0)
    slash_interval = raw.get("slash_bass_interval")
    if degree is not None and (not isinstance(degree, int) or not 1 <= degree <= 7):
        raise ValueError("degree must be 1..7 or null")
    if not isinstance(root_offset, int) or not 0 <= root_offset < 12:
        raise ValueError("root_offset must be a pitch class in 0..11")
    if not isinstance(quality_id, int) or quality_id not in quality_by_id:
        raise ValueError("quality_id is absent from the pinned registry")
    if not isinstance(accidental, int) or accidental not in (-2, -1, 0, 1, 2):
        raise ValueError("accidental must be in -2..2")
    if slash_interval is not None and (
        not isinstance(slash_interval, int) or not 0 <= slash_interval < 12
    ):
        raise ValueError("slash_bass_interval must be null or 0..11")

    quality = quality_by_id[quality_id]
    interval_mask = _mask(quality["intervals"])
    skeleton_mask = _mask(quality["skeleton"])
    extension_flags = interval_mask & ~skeleton_mask & 0x0FFF
    alteration_flags = accidental + 2
    skeleton = tuple(quality["skeleton"])
    if 6 in skeleton and 7 not in skeleton:
        alteration_flags |= 1 << 3
    if 8 in skeleton and 7 not in skeleton:
        alteration_flags |= 1 << 4
    if skeleton == (0, 2, 7):
        alteration_flags |= 1 << 5
    if skeleton == (0, 5, 7):
        alteration_flags |= 1 << 6
    if skeleton == (0, 7):
        alteration_flags |= 1 << 7

    return (
        255 if degree is None else degree - 1,
        ((root_offset + 6) % 12) - 6,
        _quality_group(quality),
        extension_flags,
        alteration_flags,
        255 if slash_interval is None else slash_interval,
        0,  # Reserved spelling flags; v1 groups semantic function, not source notation.
    )


def _token_bytes(token):
    degree, root, group, extensions, alterations, slash, spelling = token
    return struct.pack("<BbBHBBB", degree, root, group, extensions,
                       alterations, slash, spelling)


def _token_sort_key(token):
    return (token[0], token[1], token[2], token[3], token[4], token[5], token[6])


def _count_add(counter, key, count=1):
    if count <= 0:
        raise ValueError("transition count must be positive")
    counter[key] += count


def _cadence_name(tokens):
    if not tokens:
        return "none"
    last_degree = tokens[-1][0]
    prior_degree = tokens[-2][0] if len(tokens) >= 2 else 255
    if last_degree == 0 and prior_degree == 4:
        return "authentic"
    if last_degree == 0 and prior_degree == 3:
        return "plagal"
    if last_degree == 5 and prior_degree == 4:
        return "deceptive"
    if last_degree == 4:
        return "half"
    return "other"


def _is_modal_route(route):
    for locator in route.get("source_locators", []):
        if locator.get("list") == "prog_modal" or locator.get("source_dialect") == "modal":
            return True
    return False


def _content_bytes(stats):
    body = bytearray(b"HGS-TRANSITION-STATS\x00\x01")
    source_digest = stats["source_model_content_sha256"]
    body.extend(bytes.fromhex(source_digest) if source_digest else bytes(32))
    tokens = stats["tokens"]
    body.extend(struct.pack("<H", len(tokens)))
    for row in tokens:
        body.extend(struct.pack("<H", row["id"]))
        body.extend(_token_bytes(tuple(row["token"])))
        body.extend(struct.pack("<I", row["unigram_count"]))
    for field, marker, fmt in (
        ("bigrams", b"B", "<HHI"),
        ("trigrams", b"T", "<HHHI"),
        ("quality_transitions", b"Q", "<BBI"),
        ("root_motion_quality", b"R", "<BBBI"),
        ("modal_borrowed_edges", b"D", "<HHI"),
    ):
        rows = stats[field]
        body.extend(marker)
        body.extend(struct.pack("<I", len(rows)))
        for row in rows:
            if field == "bigrams":
                values = (row["from"], row["to"], row["count"])
            elif field == "trigrams":
                values = (row["previous2"], row["previous1"], row["target"], row["count"])
            elif field == "quality_transitions":
                values = (row["from_group"], row["to_group"], row["count"])
            elif field == "root_motion_quality":
                values = (row["from_group"], row["root_motion_class"],
                          row["to_group"], row["count"])
            else:
                values = (row["from"], row["to"], row["count"])
            body.extend(struct.pack(fmt, *values))
    body.extend(b"C")
    cadence_names = sorted(stats["cadence_counts"])
    body.extend(struct.pack("<H", len(cadence_names)))
    for name in cadence_names:
        encoded = name.encode("ascii")
        body.extend(struct.pack("<B", len(encoded)))
        body.extend(encoded)
        body.extend(struct.pack("<I", stats["cadence_counts"][name]))
    body.extend(b"M")
    body.extend(struct.pack("<II", stats["modal_borrowed_edge_count"],
                            stats["raw_route_weight"]))
    body.extend(b"D")
    modal_rows = stats["modal_borrowed_edges"]
    body.extend(struct.pack("<I", len(modal_rows)))
    for row in modal_rows:
        body.extend(struct.pack("<HHI", row["from"], row["to"], row["count"]))
    return bytes(body)


def compile_transition_stats(model):
    """Count each unique route once; preserve repeated events within that route."""
    if not isinstance(model, dict) or model.get("schema_version") != 1:
        raise ValueError("expected normalized harmony manifest schema v1")
    corpus = model.get("corpus")
    if not isinstance(corpus, dict) or not isinstance(corpus.get("routes"), list):
        raise ValueError("model has no normalized corpus routes")
    quality_rows = model.get("quality_registry")
    if not isinstance(quality_rows, list):
        raise ValueError("model has no quality registry")
    quality_by_id = {row.get("id"): row for row in quality_rows}
    if len(quality_by_id) != len(quality_rows):
        raise ValueError("quality registry IDs must be unique")

    route_ids = set()
    token_counts = Counter()
    bigram_counts = Counter()
    trigram_counts = Counter()
    quality_counts = Counter()
    root_quality_counts = Counter()
    modal_counts = Counter()
    cadence_counts = Counter()
    modal_edge_count = 0
    raw_route_weight = 0
    for route in corpus["routes"]:
        route_id = route.get("id")
        if not isinstance(route_id, str) or not route_id or route_id in route_ids:
            raise ValueError("route IDs must be unique nonempty strings")
        route_ids.add(route_id)
        if route.get("weight") != 1:
            raise ValueError("corpus routes must carry exactly one statistical vote")
        raw_route_weight += 1
        tokens = [_canonical_token(item, quality_by_id) for item in route.get("tokens", [])]
        if not tokens:
            raise ValueError(f"route {route_id} has no tokens")
        for item in tokens:
            _count_add(token_counts, item)
        for left, right in zip(tokens, tokens[1:]):
            _count_add(bigram_counts, (left, right))
            source_group, target_group = left[2], right[2]
            _count_add(quality_counts, (source_group, target_group))
            motion = (right[1] - left[1]) % 12
            motion_class = min(motion, 12 - motion)
            _count_add(root_quality_counts, (source_group, motion_class, target_group))
            if right[4] != 2 or _is_modal_route(route):
                modal_edge_count += 1
                _count_add(modal_counts, (left, right))
        for first, second, target in zip(tokens, tokens[1:], tokens[2:]):
            _count_add(trigram_counts, (first, second, target))
        cadence_counts[_cadence_name(tokens)] += 1

    ordered_tokens = sorted(token_counts, key=_token_sort_key)
    token_id = {token: index for index, token in enumerate(ordered_tokens)}
    if len(ordered_tokens) > 0xFFFF:
        raise ValueError("canonical token registry exceeds uint16 ID capacity")
    bigrams = [
        {"from": token_id[left], "to": token_id[right], "count": count}
        for (left, right), count in sorted(
            bigram_counts.items(), key=lambda item: (token_id[item[0][0]], token_id[item[0][1]])
        )
    ]
    trigrams = [
        {"previous2": token_id[a], "previous1": token_id[b],
         "target": token_id[target], "count": count}
        for (a, b, target), count in sorted(
            trigram_counts.items(),
            key=lambda item: (token_id[item[0][0]], token_id[item[0][1]], token_id[item[0][2]])
        )
    ]
    modal_edges = [
        {"from": token_id[left], "to": token_id[right], "count": count}
        for (left, right), count in sorted(
            modal_counts.items(),
            key=lambda item: (token_id[item[0][0]], token_id[item[0][1]])
        )
    ]
    stats = {
        "schema_version": 1,
        "token_schema": TOKEN_SCHEMA,
        "token_bytes": TOKEN_SIZE,
        "tokens": [{"id": token_id[token], "token": list(token),
                    "unigram_count": token_counts[token]}
                   for token in ordered_tokens],
        "unigrams": [{"token": list(token), "count": token_counts[token]}
                     for token in ordered_tokens],
        "bigrams": bigrams,
        "trigrams": trigrams,
        "quality_transitions": [
            {"from_group": source, "to_group": target, "count": count}
            for (source, target), count in sorted(quality_counts.items())
        ],
        "root_motion_quality": [
            {"from_group": source, "root_motion_class": motion,
             "to_group": target, "count": count}
            for (source, motion, target), count in sorted(root_quality_counts.items())
        ],
        "modal_borrowed_edges": modal_edges,
        "cadence_counts": {name: cadence_counts[name] for name in sorted(cadence_counts)},
        "modal_borrowed_edge_count": modal_edge_count,
        "mood_tag_count_used": 0,
        "source_vocabulary_count_used": 0,
        "raw_route_weight": raw_route_weight,
        "route_count": len(route_ids),
        "source_model_content_sha256": model.get("content_sha256", ""),
        "source_lock_sha256": model.get("provenance", {}).get("source_lock_sha256", ""),
        "source_commits": {
            name: source.get("commit", "")
            for name, source in sorted(model.get("provenance", {}).get("sources", {}).items())
        },
        "source_roles": {
            name: source.get("role", "")
            for name, source in sorted(model.get("provenance", {}).get("sources", {}).items())
        },
    }
    source_hash = stats["source_model_content_sha256"]
    if source_hash and (len(source_hash) != 64 or
                        any(character not in "0123456789abcdef" for character in source_hash)):
        raise ValueError("source model content checksum must be lowercase SHA-256")
    stats["content_sha256"] = hashlib.sha256(_content_bytes(stats)).hexdigest()
    validate_stats(stats)
    return stats


def quantize_successors(successors):
    """Return (id, prior) pairs; zero counts stay zero, observed rows scale to 1..15."""
    rows = list(successors)
    if any(not isinstance(target, int) or target < 0 or
           not isinstance(count, int) or count < 0 for target, count in rows):
        raise ValueError("successor IDs/counts must be nonnegative integers")
    if len({target for target, _ in rows}) != len(rows):
        raise ValueError("successor IDs must be unique per context")
    maximum = max((count for _, count in rows), default=0)
    output = []
    for target, count in rows:
        if count == 0 or maximum == 0:
            prior = 0
        else:
            prior = max(1, (30 * count + maximum) // (2 * maximum))
            prior = min(15, prior)
        output.append((target, prior))
    return sorted(output)


def top_n_report(trigram_contexts, choices=(2, 4, 8)):
    """Report successor coverage and estimated table bytes for candidate top-N values."""
    result = {}
    observed = {context: [(target, count) for target, count in rows if count > 0]
                for context, rows in trigram_contexts.items()}
    all_unique = sum(len(rows) for rows in observed.values())
    all_count = sum(count for rows in observed.values() for _, count in rows)
    token_count = len({token for context, rows in trigram_contexts.items()
                       for token in (*context,
                                     *(target for target, count in rows if count > 0))})
    for limit in choices:
        retained_unique = 0
        retained_count = 0
        context_count = sum(bool(rows) for rows in observed.values())
        truncated_contexts = 0
        for rows in observed.values():
            ordered = sorted(rows, key=lambda row: (-row[1], row[0]))
            retained = ordered[:limit]
            retained_unique += len(retained)
            retained_count += sum(count for _, count in retained)
            if len(ordered) > limit:
                truncated_contexts += 1
        result[str(limit)] = {
            "top_n": limit,
            "context_count": context_count,
            "truncated_context_count": truncated_contexts,
            "retained_unique_successors": retained_unique,
            "truncated_unique_successors": all_unique - retained_unique,
            "unique_successor_coverage_num": retained_unique,
            "unique_successor_coverage_den": all_unique,
            "weighted_coverage_num": retained_count,
            "weighted_coverage_den": all_count,
            "estimated_token_bytes": token_count * TOKEN_SIZE,
            "estimated_trigram_entry_bytes": retained_unique * 7,
            "estimated_token_and_trigram_bytes": token_count * TOKEN_SIZE + retained_unique * 7,
        }
    return result


def _trigram_contexts(stats):
    contexts = defaultdict(list)
    for row in stats["trigrams"]:
        contexts[(row["previous2"], row["previous1"])].append(
            (row["target"], row["count"])
        )
    return contexts


def validate_stats(stats):
    if not isinstance(stats, dict) or stats.get("schema_version") != 1:
        raise ValueError("unsupported transition stats schema")
    if stats.get("token_schema") != TOKEN_SCHEMA or stats.get("token_bytes") != TOKEN_SIZE:
        raise ValueError("canonical token schema mismatch")
    source_hash = stats.get("source_model_content_sha256")
    if not isinstance(source_hash, str) or (source_hash and len(source_hash) != 64):
        raise ValueError("invalid source model checksum")
    tokens = stats.get("tokens")
    if not isinstance(tokens, list) or len(tokens) > 0xFFFF:
        raise ValueError("invalid canonical token table")
    if [row.get("id") for row in tokens] != list(range(len(tokens))):
        raise ValueError("canonical token IDs must be contiguous")
    tuples = [tuple(row.get("token", [])) for row in tokens]
    if tuples != sorted(tuples, key=_token_sort_key) or len(set(tuples)) != len(tuples):
        raise ValueError("canonical tokens must be unique and content sorted")
    if any(len(item) != 7 for item in tuples):
        raise ValueError("canonical token must contain seven explicit fields")
    for name, width in (("bigrams", 2), ("trigrams", 3),
                        ("modal_borrowed_edges", 2)):
        rows = stats.get(name)
        if not isinstance(rows, list):
            raise ValueError(f"missing {name}")
        previous = None
        for row in rows:
            if name in ("bigrams", "modal_borrowed_edges"):
                key = (row["from"], row["to"])
            else:
                key = (row["previous2"], row["previous1"], row["target"])
            if len(key) != width or any(not isinstance(value, int) or
                                         not 0 <= value < len(tokens) for value in key):
                raise ValueError(f"invalid {name} token ID")
            if previous is not None and key <= previous:
                raise ValueError(f"{name} keys must be sorted and unique")
            if not isinstance(row.get("count"), int) or row["count"] <= 0:
                raise ValueError(f"{name} count must be positive")
            previous = key
    expected = hashlib.sha256(_content_bytes(stats)).hexdigest()
    if stats.get("content_sha256") != expected:
        raise ValueError("transition stats checksum mismatch")
    return stats


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--model", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()
    model = json.loads(args.model.read_text(encoding="utf-8"))
    stats = compile_transition_stats(model)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(stats, indent=2, sort_keys=True) + "\n",
                        encoding="utf-8")


if __name__ == "__main__":
    main()
