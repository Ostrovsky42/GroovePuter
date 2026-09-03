#!/usr/bin/env python3
"""Contracts for portable frozen Git history boundaries."""

from __future__ import annotations

import subprocess
import tempfile
import unittest
from pathlib import Path

from gf2_frozen_git_boundary import resolve_frozen_commit


class FrozenGitBoundaryTests(unittest.TestCase):
    def test_resolves_server_authored_commit_by_tree_and_subject(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            subprocess.run(["git", "init", "-q"], cwd=root, check=True)
            subprocess.run(
                ["git", "config", "user.email", "test@example.com"],
                cwd=root,
                check=True,
            )
            subprocess.run(
                ["git", "config", "user.name", "Test"], cwd=root, check=True
            )
            (root / "artifact.txt").write_text("frozen\n", encoding="utf-8")
            subprocess.run(["git", "add", "artifact.txt"], cwd=root, check=True)
            subprocess.run(
                ["git", "commit", "-q", "-m", "frozen boundary"],
                cwd=root,
                check=True,
            )
            boundary = subprocess.run(
                ["git", "rev-parse", "HEAD"],
                cwd=root,
                check=True,
                capture_output=True,
                text=True,
            ).stdout.strip()
            tree = subprocess.run(
                ["git", "rev-parse", "HEAD^{tree}"],
                cwd=root,
                check=True,
                capture_output=True,
                text=True,
            ).stdout.strip()
            (root / "later.txt").write_text("later\n", encoding="utf-8")
            subprocess.run(["git", "add", "later.txt"], cwd=root, check=True)
            subprocess.run(
                ["git", "commit", "-q", "-m", "later work"],
                cwd=root,
                check=True,
            )

            resolved = resolve_frozen_commit(
                root,
                exact_sha="0" * 40,
                expected_tree=tree,
                expected_subject="frozen boundary",
            )

            self.assertEqual(resolved, boundary)


if __name__ == "__main__":
    unittest.main()
