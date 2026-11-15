#!/usr/bin/env python3
"""Build identity header stays deterministic and rejects incomplete provenance."""

from pathlib import Path
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[1]
GENERATOR = ROOT / "scripts" / "write_build_identity.py"
SOURCE_SHA = "0123456789abcdef0123456789abcdef01234567"


def run_generator(output: Path, version: str, source_sha: str, source_state: str):
    return subprocess.run(
        [
            sys.executable,
            str(GENERATOR),
            "--output",
            str(output),
            "--version",
            version,
            "--source-sha",
            source_sha,
            "--source-state",
            source_state,
        ],
        capture_output=True,
        text=True,
        check=False,
    )


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="gp-build-identity-") as temp:
        root = Path(temp)
        first = root / "first.h"
        second = root / "second.h"

        result = run_generator(first, "0.9.35-rc1", SOURCE_SHA, "clean")
        assert result.returncode == 0, (
            "valid build identity must produce a header: " + result.stderr
        )
        header = first.read_text(encoding="utf-8")
        assert 'kVersion[] = "0.9.35-rc1"' in header
        assert f'kSourceCommit[] = "{SOURCE_SHA}"' in header
        assert "kSourceDirty = false" in header

        repeated = run_generator(second, "0.9.35-rc1", SOURCE_SHA, "clean")
        assert repeated.returncode == 0, repeated.stderr
        assert second.read_bytes() == first.read_bytes(), (
            "identical provenance must produce byte-identical headers"
        )

        release_candidate = root / "release-candidate.h"
        rc_result = run_generator(
            release_candidate, "0.9.35-rc+0123456", SOURCE_SHA, "clean"
        )
        assert rc_result.returncode == 0, (
            "release branch SemVer with prerelease and build metadata must be "
            "accepted: " + rc_result.stderr
        )

        dirty = root / "dirty.h"
        dirty_result = run_generator(dirty, "0.9.35-dev", SOURCE_SHA, "dirty")
        assert dirty_result.returncode == 0, dirty_result.stderr
        assert "kSourceDirty = true" in dirty.read_text(encoding="utf-8")

        invalid = root / "invalid.h"
        rejected = run_generator(invalid, "0.9.35-rc1", "short", "clean")
        assert rejected.returncode != 0, "short source SHA must be rejected"
        assert not invalid.exists(), "invalid provenance must not leave a header"

    build_script = (ROOT / "scripts" / "build.sh").read_text(encoding="utf-8")
    sketch = (ROOT / "GroovePuter.ino").read_text(encoding="utf-8")
    fallback_header = ROOT / "src" / "build_identity_generated.h"
    assert "write_build_identity.py" in build_script, (
        "firmware build must generate identity in its staged source tree"
    )
    assert "GROOVEPUTER_BUILD_VERSION" in build_script
    assert "rev-parse --verify HEAD" in build_script
    assert '"src/build_identity_generated.h"' in sketch
    assert fallback_header.is_file(), (
        "direct Arduino IDE builds need a checked-in fallback identity header"
    )
    fallback = fallback_header.read_text(encoding="utf-8")
    assert 'kVersion[] = "0.0.0-dev"' in fallback
    assert 'kSourceCommit[] = "unknown"' in fallback
    assert "kSourceDirty = true" in fallback
    assert "[BUILD] version=%s commit=%s tree=%s" in sketch

    print("build identity header generation: PASS")


if __name__ == "__main__":
    main()
