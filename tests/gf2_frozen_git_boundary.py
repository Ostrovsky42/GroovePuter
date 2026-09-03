"""Resolve frozen research commits across metadata-only Git republishing."""

from __future__ import annotations

import subprocess
from pathlib import Path


def _git(root: Path, *args: str, check: bool = True) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["git", *args],
        cwd=root,
        check=check,
        capture_output=True,
        text=True,
    )


def resolve_frozen_commit(
    root: Path,
    *,
    exact_sha: str,
    expected_tree: str,
    expected_subject: str,
) -> str:
    """Return the exact boundary or its unique tree-and-subject equivalent."""
    exact = _git(root, "cat-file", "-e", f"{exact_sha}^{{commit}}", check=False)
    if exact.returncode == 0:
        ancestor = _git(
            root,
            "merge-base",
            "--is-ancestor",
            exact_sha,
            "HEAD",
            check=False,
        )
        if ancestor.returncode == 0:
            tree = _git(root, "rev-parse", f"{exact_sha}^{{tree}}").stdout.strip()
            subject = _git(root, "show", "-s", "--format=%s", exact_sha).stdout.strip()
            if tree != expected_tree or subject != expected_subject:
                raise RuntimeError(f"frozen commit metadata mismatch: {exact_sha}")
            return exact_sha

    candidates = []
    for line in _git(root, "log", "--format=%H%x09%T%x09%s", "HEAD").stdout.splitlines():
        sha, tree, subject = line.split("\t", 2)
        if tree == expected_tree and subject == expected_subject:
            candidates.append(sha)
    if len(candidates) != 1:
        raise RuntimeError(
            "expected one frozen commit equivalent for "
            f"{exact_sha}, found {len(candidates)}"
        )
    return candidates[0]
