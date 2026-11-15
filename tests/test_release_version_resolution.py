#!/usr/bin/env python3
"""Release refs resolve to deterministic firmware semantic versions."""

from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
RESOLVER = ROOT / "scripts" / "resolve_build_version.py"
SOURCE_SHA = "0123456789abcdef0123456789abcdef01234567"


def resolve(ref_name: str, ref_type: str, source_sha: str = SOURCE_SHA):
    return subprocess.run(
        [
            sys.executable,
            str(RESOLVER),
            "--ref-name",
            ref_name,
            "--ref-type",
            ref_type,
            "--source-sha",
            source_sha,
        ],
        capture_output=True,
        text=True,
        check=False,
    )


def main() -> None:
    stable = resolve("v0.9.35", "tag")
    assert stable.returncode == 0, stable.stderr
    assert stable.stdout.strip() == "GROOVEPUTER_BUILD_VERSION=0.9.35"

    prerelease = resolve("v0.9.35-rc1", "tag")
    assert prerelease.returncode == 0, prerelease.stderr
    assert prerelease.stdout.strip() == "GROOVEPUTER_BUILD_VERSION=0.9.35-rc1"

    branch = resolve("release/0.9.35", "branch")
    assert branch.returncode == 0, branch.stderr
    assert branch.stdout.strip() == (
        "GROOVEPUTER_BUILD_VERSION=0.9.35-rc+0123456"
    )

    dispatch = resolve("main", "branch")
    assert dispatch.returncode == 0, dispatch.stderr
    assert dispatch.stdout.strip() == (
        "GROOVEPUTER_BUILD_VERSION=0.0.0-dev+0123456"
    )

    invalid = resolve("v0.9.35", "tag", "short")
    assert invalid.returncode != 0, "short source SHA must be rejected"

    workflow = (ROOT / ".github" / "workflows" / "release.yml").read_text(
        encoding="utf-8"
    )
    assert "scripts/resolve_build_version.py" in workflow, (
        "release workflow must use the tested version resolver"
    )
    assert "GITHUB_ENV" in workflow

    print("release build version resolution: PASS")


if __name__ == "__main__":
    main()
