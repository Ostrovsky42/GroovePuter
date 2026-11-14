import hashlib
import importlib.util
import json
import pathlib
import tempfile
import unittest
import zipfile


ROOT = pathlib.Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts/package_hardware_candidate.py"
SPEC = importlib.util.spec_from_file_location("candidate_packager", SCRIPT)
packager = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(packager)


class CandidatePackagingTests(unittest.TestCase):
    def make_build(self, root, app_offset="0x10000"):
        build = root / "build"
        generated = build / ".arduino-build"
        generated.mkdir(parents=True)
        (generated / "partitions.csv").write_text(
            "# Name, Type, SubType, Offset, Size, Flags\n"
            f"app0, app, ota_0, {app_offset}, 0x300000,\n",
            encoding="utf-8",
        )
        (generated / "sdkconfig").write_text(
            "CONFIG_PARTITION_TABLE_OFFSET=0x8000\n", encoding="utf-8"
        )
        (root / "platform.txt").write_text(
            "build.flash_size=4MB\n"
            "build.bootloader_addr=0x1000\n"
            "recipe.hooks.objcopy.postobjcopy.3.pattern_args=merge-bin "
            "0x8000 \"{build.path}/{build.project_name}.partitions.bin\" "
            "0xe000 \"{runtime.platform.path}/tools/partitions/boot_app0.bin\" "
            "0x10000 \"{build.path}/{build.project_name}.bin\"\n",
            encoding="utf-8",
        )
        (root / "boot_app0.bin").write_bytes(b"boot-app0")
        (root / "build.log").write_text(
            "Sketch uses 123,456 bytes (4%) of program storage space.\n"
            "Global variables use 187,636 bytes (57%) of dynamic memory, "
            "leaving 140,044 bytes for local variables.\n",
            encoding="utf-8",
        )
        for name, data in {
            "GroovePuter.ino.bin": b"application",
            "GroovePuter.ino.merged.bin": bytes([0xA5]) * (4 * 1024 * 1024),
            "GroovePuter.ino.bootloader.bin": b"bootloader",
            "GroovePuter.ino.partitions.bin": b"partition-table",
            "GroovePuter.ino.elf": b"elf",
            "GroovePuter.ino.map": b"map",
        }.items():
            (build / name).write_bytes(data)
        return build

    def package(self, root, build, out_name):
        return packager.prepare_candidate_package(
            build_dir=build,
            output_dir=root / out_name,
            version="0.9.34",
            source_sha="a" * 40,
            fqbn="m5stack:esp32:m5stack_cardputer:PSRAM=disabled",
            core_version="m5stack:esp32@3.2.2",
            arduino_cli_version="arduino-cli 1.4.0",
            esp_idf_commit="858a988d6eb90f54661abe282c523879c6ad0116",
            platform_txt=root / "platform.txt",
            boot_app0=root / "boot_app0.bin",
            build_log=root / "build.log",
        )

    def test_manifest_addresses_checksums_and_legacy_m5burner_structure(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            build = self.make_build(root)
            output, archive = self.package(root, build, "package")
            manifest = json.loads((output / "manifest.json").read_text())

            self.assertEqual(manifest["source"]["commit"], "a" * 40)
            self.assertEqual(manifest["memory"]["program_storage_bytes"], 123456)
            self.assertEqual(manifest["memory"]["static_dram_bytes"], 187636)
            self.assertEqual(manifest["memory"]["dram_headroom_bytes"], 3852)
            self.assertEqual(manifest["flash"]["app_partition"]["offset"], "0x10000")
            self.assertEqual(manifest["flash"]["merged_image"]["offset"], "0x0")
            self.assertEqual(manifest["compatibility"]["m5launcher"], "HARDWARE_UNVERIFIED")

            burner = output / "m5burner-dry-run"
            metadata = json.loads((burner / "m5burner.json").read_text())
            category = metadata["firmware_category"][0]["Cardputer ADV"]
            self.assertEqual(category["path"], "firmware")
            names = sorted(path.name for path in (burner / "firmware").iterdir())
            self.assertEqual(names, [
                "GroovePuter_0x10000.bin",
                "boot_app0_0xe000.bin",
                "bootloader_0x1000.bin",
                "partitions_0x8000.bin",
            ])
            self.assertIn("has not been opened in M5Burner", (burner / "README.md").read_text())

            app = output / "artifacts/GroovePuter.ino.bin"
            app_hash = hashlib.sha256(app.read_bytes()).hexdigest()
            self.assertTrue(any(
                item["path"] == "artifacts/GroovePuter.ino.bin"
                and item["sha256"] == app_hash
                for item in manifest["artifacts"]
            ))
            self.assertTrue(archive.is_file())
            sidecar = archive.with_name(archive.name + ".sha256").read_text()
            self.assertTrue(sidecar.startswith(hashlib.sha256(archive.read_bytes()).hexdigest()))
            with zipfile.ZipFile(archive) as zipped:
                self.assertIn("manifest.json", zipped.namelist())
                self.assertIn("m5burner-dry-run/m5burner.json", zipped.namelist())

    def test_package_zip_is_reproducible(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            build = self.make_build(root)
            _, first = self.package(root, build, "candidate-0.9.35-first")
            _, second = self.package(root, build, "candidate-0.9.35-second")
            self.assertEqual(first.name, "candidate-0.9.35-first.zip")
            self.assertEqual(first.read_bytes(), second.read_bytes())

    def test_rejects_partition_table_that_disagrees_with_flash_recipe(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            build = self.make_build(root, app_offset="0x20000")
            with self.assertRaisesRegex(ValueError, "app partition offset"):
                self.package(root, build, "package")


if __name__ == "__main__":
    unittest.main()
