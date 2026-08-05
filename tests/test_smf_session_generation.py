#!/usr/bin/env python3
import os
import subprocess
from pathlib import Path

ROOT = Path.cwd()
BUILD_DIR = ROOT / "build" / "host-tests"


def main() -> None:
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    binary = BUILD_DIR / "test_smf_session_generation"
    cxx = os.environ.get("CXX", "g++")
    subprocess.run(
        [
            cxx,
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            f"-I{ROOT}",
            str(ROOT / "tests/test_smf_session_generation.cpp"),
            str(ROOT / "src/midi/smf_stream.cpp"),
            "-o",
            str(binary),
        ],
        check=True,
    )
    subprocess.run([str(binary)], check=True)
    print("SMF session generation host tests: OK")


if __name__ == "__main__":
    main()
