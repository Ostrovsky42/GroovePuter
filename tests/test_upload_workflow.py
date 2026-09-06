"""Exercise upload ordering with a fake toolchain; never access hardware."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class UploadWorkflowTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="grooveputer-upload-test-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        scripts = self.root / "scripts"
        scripts.mkdir()
        shutil.copy(ROOT / "scripts/upload.sh", scripts)
        self.events = self.root / "events"
        self.build = self.root / "build with spaces"
        self.build.mkdir()
        (self.build / "GroovePuter.ino.bin").write_text("stale")
        self.env = dict(os.environ, BUILD_PATH=str(self.build),
                        FQBN="test:board:profile", EVENTS=str(self.events))
        self.stub(scripts / "build.sh", 'echo "build:$FQBN" >> "$EVENTS"\n'
                  'exit "${BUILD_RESULT:-0}"\n')
        self.stub(scripts / "check_cardputer_dram_budget.sh",
                  'echo "budget:$1" >> "$EVENTS"\n'
                  'exit "${BUDGET_RESULT:-0}"\n')
        cli = self.root / "fake-arduino"
        self.stub(cli, 'printf "cli:%s\\n" "$*" >> "$EVENTS"\n'
                  'exit "${UPLOAD_RESULT:-0}"\n')
        self.env["ARDUINO_CLI"] = str(cli)

    def stub(self, path, body):
        path.write_text("#!/usr/bin/env bash\nset -eu\n" + body)
        path.chmod(0o755)

    def run_upload(self, *args):
        return subprocess.run(["bash", str(self.root / "scripts/upload.sh"), *args],
                              env=self.env, cwd=self.temp.name,
                              capture_output=True, text=True)

    def recorded(self):
        return self.events.read_text().splitlines() if self.events.exists() else []

    def test_default_builds_and_checks_before_upload(self):
        result = self.run_upload("/dev/test-cardputer")
        self.assertEqual(result.returncode, 0, result.stderr)
        events = self.recorded()
        self.assertEqual(events[0], "build:test:board:profile")
        self.assertEqual(events[1], f"budget:{self.build}/GroovePuter.ino.elf")
        self.assertIn("cli:upload --fqbn test:board:profile", events[2])
        self.assertIn("-p /dev/test-cardputer", events[2])

    def test_failed_build_never_uploads_stale_binary(self):
        self.env["BUILD_RESULT"] = "17"
        self.assertEqual(self.run_upload().returncode, 17)
        self.assertEqual(self.recorded(), ["build:test:board:profile"])

    def test_failed_budget_never_uploads(self):
        self.env["BUDGET_RESULT"] = "18"
        self.assertEqual(self.run_upload().returncode, 18)
        self.assertEqual(len(self.recorded()), 2)

    def test_missing_prebuilt_does_not_fall_back(self):
        self.assertNotEqual(self.run_upload("--prebuilt").returncode, 0)
        self.assertEqual(self.recorded(), [])

    def test_unknown_option_fails_before_build_or_upload(self):
        self.assertNotEqual(self.run_upload("--prebuit").returncode, 0)
        self.assertEqual(self.recorded(), [])

    def test_upload_failure_propagates(self):
        self.env["UPLOAD_RESULT"] = "19"
        self.assertEqual(self.run_upload().returncode, 19)


if __name__ == "__main__":
    unittest.main()
