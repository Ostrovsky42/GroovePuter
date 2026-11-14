import hashlib
import importlib.util
import json
import pathlib
import subprocess
import tempfile
import unittest
import zipfile


ROOT = pathlib.Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts/package_hardware_candidate.py"
SPEC = importlib.util.spec_from_file_location("candidate_packager", SCRIPT)
packager = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(packager)
CARDPUTER_FQBN = (
    "m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,"
    "USBMode=default,CDCOnBoot=default,UploadMode=cdc"
)


class CandidatePackagingTests(unittest.TestCase):
    def make_build(self, root, app_offset="0x10000", source_sha="a" * 40,
                   version="0.9.34"):
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
        (root / "boards.txt").write_text(
            "m5stack_cardputer.build.flash_size=8MB\n"
            "m5stack_cardputer.build.bootloader_addr=0x0\n",
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
            "GroovePuter.ino.elf": b"ELF build identity " + version.encode("ascii")
            + b" " + source_sha.encode("ascii"),
            "GroovePuter.ino.map": b"map",
        }.items():
            (build / name).write_bytes(data)
        return build

    def package(self, root, build, out_name, *, source_sha="a" * 40,
                require_clean_source=False, repository_root=None,
                rollback_archive=None, rollback_sha256=None, version="0.9.34"):
        return packager.prepare_candidate_package(
            build_dir=build,
            output_dir=root / out_name,
            version=version,
            source_sha=source_sha,
            fqbn=CARDPUTER_FQBN,
            core_version="m5stack:esp32@3.2.2",
            arduino_cli_version="arduino-cli 1.4.0",
            esp_idf_commit="858a988d6eb90f54661abe282c523879c6ad0116",
            platform_txt=root / "platform.txt",
            boot_app0=root / "boot_app0.bin",
            build_log=root / "build.log",
            require_clean_source=require_clean_source,
            repository_root=repository_root,
            rollback_archive=rollback_archive,
            rollback_sha256=rollback_sha256,
        )

    def make_rollback_archive(self, root):
        archive = root / "rollback.zip"
        files = {
            "rollback/product/GroovePuter.ino.bin": b"accepted-app-image",
            "rollback/product/GroovePuter.ino.merged.bin": b"accepted-full-image",
            "rollback/product/GroovePuter.ino.bootloader.bin": b"accepted-bootloader",
            "rollback/product/GroovePuter.ino.partitions.bin": b"accepted-partitions",
            "rollback/product/provenance.txt": (
                "source_commit=" + "f" * 40 + f"\nfqbn={CARDPUTER_FQBN}\n"
            ).encode("utf-8"),
            "rollback/HARDWARE-ACCEPTANCE.txt": (
                "owner declared hardware acceptance PASS\n"
            ).encode("utf-8"),
        }
        checksums = "".join(
            f"{hashlib.sha256(data).hexdigest()}  {path.rsplit('/', 1)[-1]}\n"
            for path, data in files.items()
            if "/product/" in path and path.endswith(".bin")
        )
        files["rollback/product/SHA256SUMS.txt"] = checksums.encode("ascii")
        with zipfile.ZipFile(archive, "w") as zipped:
            for name, contents in files.items():
                zipped.writestr(name, contents)
        return archive

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
            self.assertEqual(manifest["flash"]["components"][0]["offset"], "0x0")
            self.assertEqual(manifest["flash"]["target_device_flash_bytes"], 8 * 1024 * 1024)
            self.assertEqual(manifest["flash"]["merged_image"]["size_bytes"], 4 * 1024 * 1024)
            self.assertEqual(manifest["compatibility"]["m5launcher"], "HARDWARE_UNVERIFIED")

            burner = output / "m5burner-dry-run"
            metadata = json.loads((burner / "m5burner.json").read_text())
            category = metadata["firmware_category"][0]["Cardputer ADV"]
            self.assertEqual(category["path"], "firmware")
            names = sorted(path.name for path in (burner / "firmware").iterdir())
            self.assertEqual(names, [
                "GroovePuter_0x10000.bin",
                "boot_app0_0xe000.bin",
                "bootloader_0x0.bin",
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

    def test_flash_size_menu_overrides_board_default(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            build = self.make_build(root)
            (root / "boards.txt").write_text(
                "m5stack_cardputer.build.flash_size=8MB\n"
                "m5stack_cardputer.build.bootloader_addr=0x0\n"
                "m5stack_cardputer.menu.FlashSize.4M.build.flash_size=4MB\n",
                encoding="utf-8",
            )
            layout = packager.flash_layout(
                build,
                root / "platform.txt",
                CARDPUTER_FQBN + ",FlashSize=4M",
            )
            self.assertEqual(layout["target_device_flash_bytes"], 4 * 1024 * 1024)

    def test_package_zip_is_reproducible(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            build = self.make_build(root)
            _, first = self.package(root, build, "candidate-0.9.35-first")
            _, second = self.package(root, build, "candidate-0.9.35-second")
            self.assertEqual(first.name, "candidate-0.9.35-first.zip")
            self.assertEqual(first.read_bytes(), second.read_bytes())

    def test_package_includes_hardware_checklist_and_checksums_it(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            build = self.make_build(root)
            output, archive = self.package(root, build, "candidate-handoff")
            relative_docs = ("HARDWARE_TEST_0_9_35.md",)
            handoff = output / "handoff"
            for name in relative_docs:
                with self.subTest(document=name):
                    bundled = handoff / name
                    source = ROOT / "docs/implementation/0.9.25-0.9.35" / name
                    self.assertEqual(bundled.read_bytes(), source.read_bytes())

            sums = (output / "SHA256SUMS.txt").read_text()
            for name in relative_docs:
                self.assertIn(f"handoff/{name}", sums)
            with zipfile.ZipFile(archive) as zipped:
                for name in relative_docs:
                    self.assertIn(f"handoff/{name}", zipped.namelist())
                readme = zipped.read("README.md").decode("utf-8")
                self.assertIn("handoff/HARDWARE_TEST_0_9_35.md", readme)

    def test_hardware_checklist_documents_both_flash_spans(self):
        checklist = (
            ROOT / "docs/implementation/0.9.25-0.9.35/HARDWARE_TEST_0_9_35.md"
        ).read_text(encoding="utf-8")
        self.assertIn("0x10000 artifacts/GroovePuter.ino.bin", checklist)
        self.assertIn("0x0 artifacts/GroovePuter.ino.merged.bin", checklist)
        self.assertIn("overwrites the first 4 MiB", checklist)

    def test_rejects_partition_table_that_disagrees_with_flash_recipe(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            build = self.make_build(root, app_offset="0x20000")
            with self.assertRaisesRegex(ValueError, "app partition offset"):
                self.package(root, build, "package")

    def test_rejects_elf_without_manifest_version_and_commit(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            build = self.make_build(root)
            (build / "GroovePuter.ino.elf").write_bytes(b"ELF missing identity")
            with self.assertRaisesRegex(ValueError, "ELF build identity"):
                self.package(root, build, "package")

    def test_accepts_release_candidate_semver_with_build_metadata(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            version = "0.9.35-rc+0123456"
            build = self.make_build(root, version=version)
            output, _ = self.package(
                root, build, "release-candidate", version=version
            )
            manifest = json.loads((output / "manifest.json").read_text())
            self.assertEqual(manifest["version"], version)

    def test_clean_source_gate_matches_commit_and_rejects_dirty_tree(self):
        self.assertTrue(
            callable(getattr(packager, "validate_clean_source", None)),
            "candidate packaging must validate the source commit and clean tree",
        )
        with tempfile.TemporaryDirectory() as directory:
            repo = pathlib.Path(directory) / "repo"
            repo.mkdir()
            subprocess.run(["git", "init", str(repo)], check=True, capture_output=True)
            subprocess.run(
                ["git", "-C", str(repo), "config", "user.name", "Packaging Test"],
                check=True,
            )
            subprocess.run(
                ["git", "-C", str(repo), "config", "user.email", "packaging@example.test"],
                check=True,
            )
            source_file = repo / "tracked.txt"
            source_file.write_text("committed\n", encoding="utf-8")
            subprocess.run(["git", "-C", str(repo), "add", "tracked.txt"], check=True)
            subprocess.run(
                ["git", "-C", str(repo), "commit", "-m", "fixture"],
                check=True,
                capture_output=True,
            )
            commit = subprocess.check_output(
                ["git", "-C", str(repo), "rev-parse", "HEAD"], text=True
            ).strip()

            packager.validate_clean_source(repo, commit)
            with self.assertRaisesRegex(ValueError, "does not match HEAD"):
                packager.validate_clean_source(repo, "b" * 40)

            source_file.write_text("uncommitted\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "source tree is dirty"):
                packager.validate_clean_source(repo, commit)

    def test_package_records_verified_clean_source(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            repo = root / "source"
            repo.mkdir()
            subprocess.run(["git", "init", str(repo)], check=True, capture_output=True)
            subprocess.run(
                ["git", "-C", str(repo), "config", "user.name", "Packaging Test"],
                check=True,
            )
            subprocess.run(
                ["git", "-C", str(repo), "config", "user.email", "packaging@example.test"],
                check=True,
            )
            (repo / "tracked.txt").write_text("clean\n", encoding="utf-8")
            subprocess.run(["git", "-C", str(repo), "add", "tracked.txt"], check=True)
            subprocess.run(
                ["git", "-C", str(repo), "commit", "-m", "fixture"],
                check=True,
                capture_output=True,
            )
            commit = subprocess.check_output(
                ["git", "-C", str(repo), "rev-parse", "HEAD"], text=True
            ).strip()
            build = self.make_build(root, source_sha=commit)
            rollback = self.make_rollback_archive(root)
            output, _ = self.package(
                root,
                build,
                "clean-package",
                source_sha=commit,
                require_clean_source=True,
                repository_root=repo,
                rollback_archive=rollback,
                rollback_sha256=hashlib.sha256(rollback.read_bytes()).hexdigest(),
            )
            manifest = json.loads((output / "manifest.json").read_text())
            self.assertEqual(manifest["source"]["working_tree"], "CLEAN")

    def test_includes_verified_rollback_images_and_provenance(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            build = self.make_build(root)
            rollback = self.make_rollback_archive(root)
            rollback_hash = hashlib.sha256(rollback.read_bytes()).hexdigest()
            output, _ = self.package(
                root,
                build,
                "candidate",
                rollback_archive=rollback,
                rollback_sha256=rollback_hash,
            )
            manifest = json.loads((output / "manifest.json").read_text())
            self.assertEqual(manifest["rollback"]["source_commit"], "f" * 40)
            self.assertEqual(manifest["rollback"]["archive_sha256"], rollback_hash)
            self.assertIn(
                "DIAGNOSTIC_IMAGE_OWNER_PASS",
                manifest["rollback"]["owner_hardware_acceptance"],
            )
            self.assertEqual(
                (output / "rollback/GroovePuter.ino.merged.bin").read_bytes(),
                b"accepted-full-image",
            )
            self.assertIn(
                "owner declared hardware acceptance PASS",
                (output / "rollback/HARDWARE-ACCEPTANCE.txt").read_text(),
            )
            self.assertIn("overwrites the currently installed firmware",
                          (output / "rollback/ROLLBACK.md").read_text())
            rollback_sums = (output / "rollback/SHA256SUMS.txt").read_text()
            for filename in (
                "GroovePuter.ino.bin",
                "GroovePuter.ino.merged.bin",
                "GroovePuter.ino.bootloader.bin",
                "GroovePuter.ino.partitions.bin",
            ):
                self.assertIn(filename, rollback_sums)

    def test_rejects_rollback_with_wrong_archive_or_internal_checksum(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            build = self.make_build(root)
            rollback = self.make_rollback_archive(root)
            with self.assertRaisesRegex(ValueError, "rollback archive SHA-256"):
                self.package(
                    root, build, "bad-archive-hash", rollback_archive=rollback,
                    rollback_sha256="0" * 64,
                )
            self.assertFalse((root / "bad-archive-hash").exists())

            with zipfile.ZipFile(rollback) as zipped:
                entries = {name: zipped.read(name) for name in zipped.namelist()}
            entries["rollback/product/GroovePuter.ino.bin"] = b"tampered app"
            with zipfile.ZipFile(rollback, "w") as zipped:
                for name, contents in entries.items():
                    zipped.writestr(name, contents)
            rollback_hash = hashlib.sha256(rollback.read_bytes()).hexdigest()
            with self.assertRaisesRegex(ValueError, "rollback artifact checksum"):
                self.package(
                    root, build, "bad-internal-hash", rollback_archive=rollback,
                    rollback_sha256=rollback_hash,
                )
            self.assertFalse((root / "bad-internal-hash").exists())


if __name__ == "__main__":
    unittest.main()
