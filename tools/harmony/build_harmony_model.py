#!/usr/bin/env python3
"""Emit compact C++ corpus-prior tables and a deterministic size manifest."""

import argparse
import hashlib
import json
import struct
from collections import defaultdict
from pathlib import Path

from build_transition_stats import (
    TOKEN_SCHEMA,
    _token_bytes,
    quantize_successors,
    top_n_report,
    validate_stats,
)


def _encode_id(value):
    return struct.pack("<H", value)


def _quantized_groups(rows, key_fields, successor_field):
    grouped = defaultdict(list)
    for row in rows:
        key = tuple(row[field] for field in key_fields)
        grouped[key].append((row[successor_field], row["count"]))
    output = []
    for key, successors in sorted(grouped.items()):
        for target, prior in quantize_successors(successors):
            if prior:
                output.append((key, target, prior))
    return output


def compile_runtime_model(stats, top_n):
    validate_stats(stats)
    if top_n not in (2, 4, 8):
        raise ValueError("top_n must be one of the measured choices 2, 4, or 8")

    bigram_rows = _quantized_groups(stats["bigrams"], ["from"], "to")
    raw_contexts = defaultdict(list)
    for row in stats["trigrams"]:
        raw_contexts[(row["previous2"], row["previous1"])].append(
            (row["target"], row["count"])
        )
    selected_trigrams = []
    for key, successors in sorted(raw_contexts.items()):
        priors = dict(quantize_successors(successors))
        for target, _count in sorted(successors, key=lambda row: (-row[1], row[0]))[:top_n]:
            prior = priors[target]
            selected_trigrams.append((key[0], key[1], target, prior))

    coarse_rows = _quantized_groups(
        stats["root_motion_quality"], ["from_group", "root_motion_class"], "to_group"
    )

    # Report N coverage from unquantized counts, not rounded priors.
    evidence = top_n_report(raw_contexts)

    trigram_bytes = bytearray()
    for previous2, previous1, target, prior in selected_trigrams:
        trigram_bytes.extend(_encode_id(previous2))
        trigram_bytes.extend(_encode_id(previous1))
        trigram_bytes.extend(_encode_id(target))
        trigram_bytes.append(prior)
    bigram_bytes = bytearray()
    for (previous,), target, prior in bigram_rows:
        bigram_bytes.extend(_encode_id(previous))
        bigram_bytes.extend(_encode_id(target))
        bigram_bytes.append(prior)
    coarse_bytes = bytearray()
    for (from_group, motion), target_group, prior in coarse_rows:
        coarse_bytes.extend((from_group, motion, target_group, prior))

    tokens = [tuple(row["token"]) for row in stats["tokens"]]
    token_bytes = b"".join(_token_bytes(token) for token in tokens)
    content = bytearray(b"HGS-CORPUS-PRIOR\x00\x01")
    content.extend(struct.pack("<B", top_n))
    content.extend(bytes.fromhex(stats["content_sha256"]))
    for blob in (token_bytes, bytes(trigram_bytes), bytes(bigram_bytes), bytes(coarse_bytes)):
        content.extend(struct.pack("<I", len(blob)))
        content.extend(blob)
    content_sha256 = hashlib.sha256(content).hexdigest()

    return {
        "top_n": top_n,
        "tokens": tokens,
        "trigram_rows": selected_trigrams,
        "bigram_rows": bigram_rows,
        "coarse_rows": coarse_rows,
        "trigram_bytes": bytes(trigram_bytes),
        "bigram_bytes": bytes(bigram_bytes),
        "coarse_bytes": bytes(coarse_bytes),
        "content_sha256": content_sha256,
        "evidence": evidence,
        "source_stats_sha256": stats["content_sha256"],
        "source_model_content_sha256": stats["source_model_content_sha256"],
        "source_lock_sha256": stats["source_lock_sha256"],
        "source_commits": stats["source_commits"],
        "source_roles": stats["source_roles"],
        "counts": {
            "token": len(tokens),
            "trigram": len(selected_trigrams),
            "trigram_before_top_n": len(stats["trigrams"]),
            "bigram": len(bigram_rows),
            "coarse": len(coarse_rows),
        },
        "flash_bytes": len(token_bytes) + len(trigram_bytes) +
                       len(bigram_bytes) + len(coarse_bytes),
        "token_schema": TOKEN_SCHEMA,
    }


def _byte_array(name, data):
    values = list(data) or [0]
    lines = [f"inline constexpr uint8_t {name}[] = {{"]
    for start in range(0, len(values), 16):
        lines.append("    " + ", ".join(f"0x{value:02x}u" for value in values[start:start + 16]) + ",")
    lines.append("};")
    return lines


def render_header(model):
    lines = [
        "#ifndef GROOVEPUTER_GENERATION_HARMONY_GENERATED_CORPUS_PRIOR_DATA_H",
        "#define GROOVEPUTER_GENERATION_HARMONY_GENERATED_CORPUS_PRIOR_DATA_H",
        "",
        '#include "../corpus_prior.h"',
        "",
        "namespace GroovePuterHarmony {",
        "namespace GeneratedCorpusPrior {",
        "",
        "inline constexpr CanonicalFunctionToken kTokens[] = {",
    ]
    for token in model["tokens"]:
        degree, root, group, extensions, alterations, slash, spelling = token
        lines.append(
            f"    {{0x{extensions:03x}u, {degree}, {root}, {group}, "
            f"0x{alterations:02x}u, {slash}, {spelling}}},"
        )
    lines += ["};", ""]
    lines += _byte_array("kTrigramBytes", model["trigram_bytes"])
    lines.append("")
    lines += _byte_array("kBigramBytes", model["bigram_bytes"])
    lines.append("")
    lines += _byte_array("kCoarseBytes", model["coarse_bytes"])
    lines += [
        "",
        "inline constexpr uint16_t kModelVersion = 2;",
        f"inline constexpr uint64_t kContentChecksum = 0x{model['content_sha256'][:16]}ull;",
        f"inline constexpr uint16_t kTokenCount = {model['counts']['token']};",
        f"inline constexpr uint16_t kTrigramCount = {model['counts']['trigram']};",
        f"inline constexpr uint16_t kBigramCount = {model['counts']['bigram']};",
        f"inline constexpr uint16_t kCoarseCount = {model['counts']['coarse']};",
        "inline constexpr CorpusModelView kModel = {",
        "    kTokens, kTokenCount, kTrigramBytes, kTrigramCount,",
        "    kBigramBytes, kBigramCount, kCoarseBytes, kCoarseCount};",
        "",
        "}  // namespace GeneratedCorpusPrior",
        "}  // namespace GroovePuterHarmony",
        "",
        "#endif  // GROOVEPUTER_GENERATION_HARMONY_GENERATED_CORPUS_PRIOR_DATA_H",
        "",
    ]
    return "\n".join(lines)


def render_manifest(model):
    return {
        "schema_version": 1,
        "token_schema": model["token_schema"],
        "source_stats_sha256": model["source_stats_sha256"],
        "source_model_content_sha256": model["source_model_content_sha256"],
        "source_lock_sha256": model["source_lock_sha256"],
        "source_commits": model["source_commits"],
        "source_roles": model["source_roles"],
        "content_sha256": model["content_sha256"],
        "selected_top_n": model["top_n"],
        "counts": model["counts"],
        "flash_bytes": model["flash_bytes"],
        "top_n_evidence": model["evidence"],
        "encoding": {
            "token_bytes": 8,
            "trigram_record_bytes": 7,
            "bigram_record_bytes": 5,
            "coarse_record_bytes": 4,
            "integer_byte_order": "little",
        },
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--stats", required=True, type=Path)
    parser.add_argument("--top-n", required=True, type=int, choices=(2, 4, 8))
    parser.add_argument("--header", required=True, type=Path)
    parser.add_argument("--manifest", required=True, type=Path)
    args = parser.parse_args()
    stats = json.loads(args.stats.read_text(encoding="utf-8"))
    runtime_model = compile_runtime_model(stats, args.top_n)
    args.header.parent.mkdir(parents=True, exist_ok=True)
    args.manifest.parent.mkdir(parents=True, exist_ok=True)
    args.header.write_text(render_header(runtime_model), encoding="utf-8")
    args.manifest.write_text(
        json.dumps(render_manifest(runtime_model), indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
