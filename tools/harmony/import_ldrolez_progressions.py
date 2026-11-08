#!/usr/bin/env python3
"""Import pinned Roman progressions without importing upstream Python modules."""

import argparse
import ast
import hashlib
import json
import subprocess
from pathlib import Path

from normalize_roman import RomanParseError, normalize_roman


ROOT = Path(__file__).resolve().parents[2]
LOCK_PATH = ROOT / "tools/harmony/source_lock.json"
GROUPS = {
    "prog_maj": ("major", "major"),
    "prog_min": ("minor", "minor"),
    "prog_modal": ("modal", "major"),
}
DOCUMENTED_UNSUPPORTED_SUFFIXES = {"M-5", "add9", "madd9"}


def _sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify_source(source_root, lock):
    source = lock["sources"]["ldrolez"]
    root = source_root.resolve()
    actual_commit = subprocess.check_output(
        ["git", "-C", str(root), "rev-parse", "HEAD"], text=True
    ).strip()
    if actual_commit != source["commit"]:
        raise ValueError(f"ldrolez source commit mismatch: {actual_commit}")
    for relative_path, expected in source["files"].items():
        if _sha256(root / relative_path) != expected:
            raise ValueError(f"ldrolez source digest mismatch: {relative_path}")
    resolver = lock["sources"]["mingus_resolver"]
    mingus_root = root.parent / "python-mingus"
    if not mingus_root.is_dir():
        raise ValueError("pinned Mingus checkout must be sibling python-mingus")
    mingus_commit = subprocess.check_output(
        ["git", "-C", str(mingus_root), "rev-parse", "0.6.1^{commit}"], text=True
    ).strip()
    if mingus_commit != resolver["commit"]:
        raise ValueError(f"Mingus 0.6.1 resolver commit mismatch: {mingus_commit}")
    for relative_path, expected in resolver["files"].items():
        actual = subprocess.check_output(
            ["git", "-C", str(mingus_root), "show", f"0.6.1:{relative_path}"],
            text=True,
        ).encode("utf-8")
        if hashlib.sha256(actual).hexdigest() != expected:
            raise ValueError(f"Mingus resolver digest mismatch: {relative_path}")
    license_bytes = subprocess.check_output(
        ["git", "-C", str(mingus_root), "show", "0.6.1:LICENSE"]
    )
    if hashlib.sha256(license_bytes).hexdigest() != resolver["license"]["repository_sha256"]:
        raise ValueError("Mingus license digest mismatch")


def extract_source_rows(source_path):
    """Read only the three pinned literal list assignments using Python AST."""
    source = source_path.read_text(encoding="utf-8")
    tree = ast.parse(source, filename=str(source_path))
    found = {}
    for node in tree.body:
        if not isinstance(node, ast.Assign) or len(node.targets) != 1:
            continue
        target = node.targets[0]
        if not isinstance(target, ast.Name) or target.id not in GROUPS:
            continue
        if target.id in found:
            raise ValueError(f"duplicate progression list {target.id}")
        if not isinstance(node.value, ast.List):
            raise ValueError(f"{target.id} must remain a literal list")
        rows = []
        for item in node.value.elts:
            if not isinstance(item, ast.Constant) or not isinstance(item.value, str):
                raise ValueError(f"{target.id} contains a non-string list item")
            raw = item.value
            route_text, marker, labels = raw.partition(" =")
            if not marker:
                labels = ""
                route_text = raw
            rows.append({
                "line": item.lineno,
                "raw": raw,
                "route_text": route_text.strip(),
                "mood_tags": labels.split(),
                "token_count": len(route_text.split()),
            })
        found[target.id] = rows
    if set(found) != set(GROUPS):
        raise ValueError(f"expected exactly progression lists {sorted(GROUPS)}")
    if sum(map(len, found.values())) != 195:
        raise ValueError("pinned source progression row count changed; review source lock")
    return found


def _unsupported_suffixes(error, source_tokens):
    """Return unsupported corpus suffixes represented in the rejected route."""
    token = error.token
    # Strip any accidental/degrees and slash bass before checking documented cases.
    for source_token in source_tokens:
        if source_token == token:
            for suffix in sorted(DOCUMENTED_UNSUPPORTED_SUFFIXES,
                                 key=lambda value: (-len(value), value)):
                if source_token.endswith(suffix):
                    return suffix
    return None


def compile_progressions(groups, source_name="chords.py"):
    accepted = []
    quarantine = []
    for group_name, rows in groups.items():
        dialect, tonality = GROUPS[group_name]
        for row in rows:
            locator = {"path": source_name, "line": row["line"], "list": group_name,
                       "index": len(accepted) + len(quarantine)}
            try:
                normalized = normalize_roman(
                    row["route_text"], {"tonality": tonality}, dialect
                )
            except RomanParseError as error:
                suffix = _unsupported_suffixes(error, row["route_text"].split())
                if error.code != "unsupported_suffix" or suffix is None:
                    raise ValueError(f"unapproved parse failure at {locator}: {error}") from error
                quarantine.append({
                    "status": "quarantined",
                    "source": locator,
                    "raw": row["raw"],
                    "route_text": row["route_text"],
                    "mood_tags": row["mood_tags"],
                    "token": error.token,
                    "reason_code": error.code,
                    "reason": error.reason,
                    "unsupported_suffix": suffix,
                })
                continue
            accepted.append({
                "status": "accepted",
                "source": locator,
                "raw": row["raw"],
                "route_text": row["route_text"],
                "mood_tags": row["mood_tags"],
                "source_dialect": normalized["source_dialect"],
                "context": normalized["context"],
                "source_tokens": normalized["source_tokens"],
                "route": normalized["route"],
            })
    return accepted, quarantine


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()
    lock = json.loads(LOCK_PATH.read_text(encoding="utf-8"))
    verify_source(args.source, lock)
    groups = extract_source_rows(args.source / "chords.py")
    accepted, quarantine = compile_progressions(groups)
    total = len(accepted) + len(quarantine)
    if total != 195:
        raise ValueError(f"accounted for {total} progression rows, expected 195")
    output = {
        "schema_version": 1,
        "sources": {
            "ldrolez_commit": lock["sources"]["ldrolez"]["commit"],
            "mingus_resolver_commit": lock["sources"]["mingus_resolver"]["commit"],
        },
        "accepted": accepted,
        "quarantine": quarantine,
    }
    content = json.dumps(output, ensure_ascii=False, sort_keys=True,
                         separators=(",", ":")) + "\n"
    digest = hashlib.sha256(content.encode("utf-8")).hexdigest()
    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / "progressions.json").write_text(content, encoding="utf-8")
    manifest = {
        "schema_version": 1,
        "ldrolez_commit": lock["sources"]["ldrolez"]["commit"],
        "ldrolez_files": lock["sources"]["ldrolez"]["files"],
        "ldrolez_chords_sha256": lock["sources"]["ldrolez"]["files"]["chords.py"],
        "mingus_resolver_commit": lock["sources"]["mingus_resolver"]["commit"],
        "mingus_resolver_files": lock["sources"]["mingus_resolver"]["files"],
        "row_count": total,
        "accepted_count": len(accepted),
        "quarantined_count": len(quarantine),
        "documented_unsupported_suffixes": sorted(DOCUMENTED_UNSUPPORTED_SUFFIXES),
        "content_sha256": digest,
    }
    (args.out / "manifest.json").write_text(
        json.dumps(manifest, sort_keys=True, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
