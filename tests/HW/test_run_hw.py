#!/usr/bin/env python3

import json
import sys
import tempfile
import unittest
from collections import Counter
from pathlib import Path
from unittest import mock

import run_hw


class HWRunnerTests(unittest.TestCase):
    def release_matrix(self):
        defaults, configured = run_hw.load_config(run_hw.DEFAULT_CONFIG)
        targets = {
            name: run_hw.merged_target(defaults, target)
            for name, target in configured.items()
        }
        return targets, run_hw.load_release_matrix(
            run_hw.DEFAULT_RELEASE_MATRIX, targets
        )

    def write_release_evidence(self, root, combinations, targets, candidate_sha):
        for combination in combinations:
            target = targets[combination["target"]]
            for repetition in range(1, combination["repeat"] + 1):
                relative = run_hw.release_evidence_path(
                    combination, target, repetition
                )
                evidence = root / relative
                evidence.mkdir(parents=True)
                protocol = (
                    target.get("hw08_qualification_protocol")
                    if combination["throughput_qualification"]
                    else target.get("hw08_protocol", "marker")
                    if combination["case"] == "HW08"
                    else "marker"
                )
                metadata = {
                    "target": combination["target"],
                    "case": combination["case"],
                    "build_type": combination["build_type"],
                    "profile": combination["profile"],
                    "up_size": combination["up_size"],
                    "float_fast": combination["float_fast"],
                    "skip_asm": combination["skip_asm"],
                    "build_only": combination["build_only"],
                    "throughput_qualification": combination[
                        "throughput_qualification"
                    ],
                    "protocol": protocol,
                    "repetition": repetition,
                    "library_sha": candidate_sha,
                    "source_dirty": False,
                    "actual_library_sha": candidate_sha,
                    "actual_library_dirty": False,
                }
                (evidence / "metadata.json").write_text(
                    json.dumps(metadata), encoding="utf-8"
                )
                result = "BUILD PASS\n" if combination["build_only"] else "PASS\n"
                (evidence / "result.txt").write_text(result, encoding="ascii")

    def test_flash_elf_selects_configured_probe(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            elf = root / "firmware.elf"
            elf.touch()
            completed = mock.Mock(
                returncode=0,
                stdout="Downloading file [/tmp/firmware.elf]...\nO.K.\n",
            )
            target = {"mcu": "STM32H7B0VB", "jlink_speed": 24000}
            with mock.patch.object(run_hw.subprocess, "run", return_value=completed) as run:
                run_hw.flash_elf(elf, target, root / "flash.log", "20721668")

            command = run.call_args[0][0]
            self.assertIn("-SelectEmuBySN", command)
            self.assertEqual(command[command.index("-SelectEmuBySN") + 1], "20721668")
            self.assertIn(f'loadfile "{elf}"', run.call_args[1]["input"])

    def test_run_flash_bypasses_project_flash_when_probe_is_selected(self):
        target = {
            "mcu": "STM32H7B0VB",
            "jlink_speed": 24000,
            "flash": {"release": [["make", "flash"]]},
        }
        with mock.patch.object(run_hw, "flash_elf") as flash_elf, mock.patch.object(
            run_hw, "run_commands"
        ) as run_commands:
            run_hw.run_flash(
                target,
                "release",
                Path("project"),
                Path("flash.log"),
                {},
                {},
                "20721668",
                Path("firmware.elf"),
                False,
            )

        flash_elf.assert_called_once()
        run_commands.assert_not_called()

    def test_project_library_rejects_revision_mismatch(self):
        with tempfile.TemporaryDirectory() as directory:
            project = Path(directory) / "project"
            library = project / "ARM_SEGGER_RTT"
            library.mkdir(parents=True)
            with mock.patch.object(
                run_hw, "git_identity", return_value=(library.resolve(), "old", False)
            ):
                with self.assertRaisesRegex(run_hw.TestFailure, "revision mismatch"):
                    run_hw.validate_project_library(project, "candidate", False)

    def test_project_library_rejects_dirty_separate_checkout(self):
        with tempfile.TemporaryDirectory() as directory:
            project = Path(directory) / "project"
            library = project / "ARM_SEGGER_RTT"
            library.mkdir(parents=True)
            with mock.patch.object(
                run_hw, "git_identity", return_value=(library.resolve(), "candidate", True)
            ):
                with self.assertRaisesRegex(run_hw.TestFailure, "project library is dirty"):
                    run_hw.validate_project_library(project, "candidate", False)

    def test_project_library_rejects_separate_checkout_for_dirty_candidate(self):
        with tempfile.TemporaryDirectory() as directory:
            project = Path(directory) / "project"
            library = project / "ARM_SEGGER_RTT"
            library.mkdir(parents=True)
            with mock.patch.object(
                run_hw,
                "git_identity",
                return_value=(library.resolve(), "candidate", False),
            ):
                with self.assertRaisesRegex(
                    run_hw.TestFailure, "runner repository is dirty"
                ):
                    run_hw.validate_project_library(
                        project, "candidate-dirty", True
                    )

    def test_project_library_allows_same_dirty_development_checkout(self):
        with tempfile.TemporaryDirectory() as directory:
            project = Path(directory) / "project"
            library = project / "ARM_SEGGER_RTT"
            library.mkdir(parents=True)
            root = library.resolve()
            with mock.patch.object(run_hw, "REPO_ROOT", root), mock.patch.object(
                run_hw, "git_identity", return_value=(root, "candidate", True)
            ):
                self.assertEqual(
                    run_hw.validate_project_library(project, "candidate-dirty", True),
                    (root, "candidate", True),
                )

    def test_all_targets_configure_debug_and_release_build_and_flash(self):
        defaults, targets = run_hw.load_config(run_hw.DEFAULT_CONFIG)
        expected = {"debug", "release"}

        self.assertEqual(defaults["toolchain_bin"], "")
        self.assertEqual(defaults["nm"], "arm-none-eabi-nm")
        for name, target in targets.items():
            with self.subTest(target=name):
                self.assertEqual(set(target["build"]), expected)
                self.assertEqual(set(target["flash"]), expected)
                self.assertFalse(Path(target.get("nm", "arm-none-eabi-nm")).is_absolute())

    def test_local_config_merges_defaults_and_target_paths(self):
        defaults, targets = run_hw.load_config(run_hw.DEFAULT_CONFIG)
        with tempfile.TemporaryDirectory() as directory:
            local = Path(directory) / "config.local.json"
            local.write_text(
                json.dumps(
                    {
                        "schema_version": 1,
                        "defaults": {"toolchain_bin": "/toolchain/bin"},
                        "targets": {
                            "h7b0_make": {"project_dir": "/projects/h7-make"}
                        },
                    }
                ),
                encoding="utf-8",
            )
            defaults, targets = run_hw.merge_local_config(defaults, targets, local)

        self.assertEqual(defaults["toolchain_bin"], "/toolchain/bin")
        self.assertEqual(targets["h7b0_make"]["project_dir"], "/projects/h7-make")
        self.assertEqual(
            targets["h7b0_cmake"]["project_dir"], "../stm32h7b0vbcmake"
        )

    def test_cli_tool_overrides_take_precedence_over_environment(self):
        targets = {"target": {"toolchain_bin": "local", "nm": "local-nm"}}
        overridden = run_hw.apply_machine_overrides(
            targets,
            "/cli/bin",
            "/cli/nm",
            {"HW_TOOLCHAIN_BIN": "/env/bin", "HW_NM": "/env/nm"},
        )
        self.assertEqual(overridden["target"]["toolchain_bin"], "/cli/bin")
        self.assertEqual(overridden["target"]["nm"], "/cli/nm")
        self.assertEqual(targets["target"]["toolchain_bin"], "local")

    def test_local_config_rejects_release_semantic_overrides(self):
        defaults, targets = run_hw.load_config(run_hw.DEFAULT_CONFIG)
        with tempfile.TemporaryDirectory() as directory:
            local = Path(directory) / "config.local.json"
            local.write_text(
                json.dumps(
                    {
                        "schema_version": 1,
                        "targets": {"h7b0_make": {"mcu": "STM32F042G6"}},
                    }
                ),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(run_hw.TestFailure, "machine paths"):
                run_hw.merge_local_config(defaults, targets, local)

    def test_environment_tool_overrides_take_precedence_over_local_config(self):
        targets = {"target": {"toolchain_bin": "local", "nm": "local-nm"}}
        overridden = run_hw.apply_machine_overrides(
            targets,
            None,
            None,
            {"HW_TOOLCHAIN_BIN": "/env/bin", "HW_NM": "/env/nm"},
        )
        self.assertEqual(overridden["target"]["toolchain_bin"], "/env/bin")
        self.assertEqual(overridden["target"]["nm"], "/env/nm")

    def test_f042_hw08_rejects_skip_asm(self):
        target = {"mcu": "STM32F042G6"}
        with self.assertRaisesRegex(run_hw.TestFailure, "does not support.*skip-asm"):
            run_hw.validate_hw08_target_options("f042_cmake", target, "HW08", 1)

    def test_f042_hw08_accepts_skip_c(self):
        target = {"mcu": "STM32F042G6"}
        run_hw.validate_hw08_target_options("f042_cmake", target, "HW08", 0)

    def test_evidence_variants_include_matrix_parameters(self):
        self.assertEqual(
            run_hw.evidence_variant("HW06", 0, 128, 0, 0, "marker", False),
            Path("up-size-128/profile-0"),
        )
        self.assertEqual(
            run_hw.evidence_variant("HW08", 0, 256, 1, 1, "marker", False),
            Path("float-fast-1_skip-asm-1/profile-0"),
        )
        self.assertEqual(
            run_hw.evidence_variant(
                "HW08", 0, 256, 0, 0, "gated-throughput", True
            ),
            Path("qualification"),
        )

    def test_matrix_evidence_variants_are_unique(self):
        hw06 = {
            run_hw.evidence_variant("HW06", 0, size, 0, 0, "marker", False)
            for size in (128, 256)
        }
        markers = {
            run_hw.evidence_variant("HW08", 0, 256, fast, asm, "marker", False)
            for fast in (0, 1)
            for asm in (0, 1)
        }
        self.assertEqual(len(hw06), 2)
        self.assertEqual(len(markers), 4)

    def test_release_matrix_matches_planned_scope(self):
        targets, combinations = self.release_matrix()
        self.assertEqual(len(combinations), 178)
        self.assertEqual(sum(item["repeat"] for item in combinations), 182)
        self.assertEqual({item["target"] for item in combinations}, set(targets))
        self.assertEqual({item["case"] for item in combinations}, set(run_hw.CASES))
        self.assertEqual(
            Counter(item["case"] for item in combinations),
            {
                "HW01": 16,
                "HW02": 8,
                "HW03": 56,
                "HW04": 14,
                "HW05": 32,
                "HW06": 16,
                "HW07": 8,
                "HW08": 28,
            },
        )

        for target_name in targets:
            hw03_profiles = {
                item["profile"]
                for item in combinations
                if item["case"] == "HW03" and item["target"] == target_name
            }
            hw05_profiles = {
                item["profile"]
                for item in combinations
                if item["case"] == "HW05" and item["target"] == target_name
            }
            hw06_sizes = {
                item["up_size"]
                for item in combinations
                if item["case"] == "HW06" and item["target"] == target_name
            }
            self.assertEqual(hw03_profiles, set(range(7)))
            self.assertEqual(hw05_profiles, set(range(4)))
            self.assertEqual(hw06_sizes, {128, 256})

        hw04_asm_targets = {
            item["target"]
            for item in combinations
            if item["case"] == "HW04" and item["profile"] == 1
        }
        self.assertEqual(
            hw04_asm_targets,
            {
                "f103_make",
                "f103_cmake",
                "f411_make",
                "f411_cmake",
                "h7b0_make",
                "h7b0_cmake",
            },
        )

        f042_hw08 = [
            item
            for item in combinations
            if item["case"] == "HW08"
            and targets[item["target"]]["mcu"] == "STM32F042G6"
        ]
        self.assertTrue(f042_hw08)
        self.assertFalse(any(item["skip_asm"] for item in f042_hw08))
        self.assertEqual(
            Counter(
                (item["build_only"], item["throughput_qualification"])
                for item in combinations
                if item["case"] == "HW08"
            ),
            {
                (False, False): 27,
                (False, True): 1,
            },
        )

    def test_release_matrix_rejects_zero_repeat(self):
        defaults, configured = run_hw.load_config(run_hw.DEFAULT_CONFIG)
        targets = {
            name: run_hw.merged_target(defaults, target)
            for name, target in configured.items()
        }
        with tempfile.TemporaryDirectory() as directory:
            matrix = Path(directory) / "matrix.json"
            matrix.write_text(
                json.dumps(
                    {
                        "schema_version": 1,
                        "entries": [
                            {
                                "case": "HW01",
                                "targets": ["f042_make"],
                                "repeat": 0,
                            }
                        ],
                    }
                ),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(run_hw.TestFailure, "at least 1"):
                run_hw.load_release_matrix(matrix, targets)

    def test_release_matrix_rejects_build_only_combinations(self):
        defaults, configured = run_hw.load_config(run_hw.DEFAULT_CONFIG)
        targets = {
            name: run_hw.merged_target(defaults, target)
            for name, target in configured.items()
        }
        with tempfile.TemporaryDirectory() as directory:
            matrix = Path(directory) / "matrix.json"
            matrix.write_text(
                json.dumps(
                    {
                        "schema_version": 1,
                        "entries": [
                            {
                                "case": "HW08",
                                "targets": ["f042_cmake"],
                                "build_only": True,
                            }
                        ],
                    }
                ),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(run_hw.TestFailure, "belong to NH10"):
                run_hw.load_release_matrix(matrix, targets)

    def test_release_matrix_mcu_order_changes_only_scheduling(self):
        targets, combinations = self.release_matrix()
        original_paths = {
            run_hw.release_evidence_path(item, targets[item["target"]], repetition)
            for item in combinations
            for repetition in range(1, item["repeat"] + 1)
        }
        ordered = run_hw.order_release_combinations(
            combinations, targets, ["STM32H7B0VB", "STM32F042G6"]
        )
        ordered_mcus = [targets[item["target"]]["mcu"] for item in ordered]
        first_f0 = ordered_mcus.index("STM32F042G6")
        first_f1 = ordered_mcus.index("STM32F103C8")
        self.assertTrue(all(mcu == "STM32H7B0VB" for mcu in ordered_mcus[:first_f0]))
        self.assertTrue(
            all(mcu == "STM32F042G6" for mcu in ordered_mcus[first_f0:first_f1])
        )
        self.assertEqual(
            {
                run_hw.release_evidence_path(
                    item, targets[item["target"]], repetition
                )
                for item in ordered
                for repetition in range(1, item["repeat"] + 1)
            },
            original_paths,
        )

    def test_release_matrix_mcu_order_rejects_unknown_and_duplicates(self):
        targets, combinations = self.release_matrix()
        with self.assertRaisesRegex(run_hw.TestFailure, "unknown --mcu-order"):
            run_hw.order_release_combinations(combinations, targets, ["unknown"])
        with self.assertRaisesRegex(run_hw.TestFailure, "duplicate --mcu-order"):
            run_hw.order_release_combinations(
                combinations, targets, ["STM32H7B0VB", "STM32H7B0VB"]
            )

    def test_release_evidence_verifier_accepts_exact_matrix(self):
        targets, combinations = self.release_matrix()
        candidate_sha = "a" * 40
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.write_release_evidence(root, combinations, targets, candidate_sha)
            self.assertEqual(
                run_hw.verify_release_evidence(
                    root, combinations, targets, candidate_sha
                ),
                182,
            )

    def test_release_evidence_verifier_rejects_missing_and_extra_files(self):
        targets, combinations = self.release_matrix()
        candidate_sha = "b" * 40
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.write_release_evidence(root, combinations, targets, candidate_sha)
            first = combinations[0]
            first_path = run_hw.release_evidence_path(first, targets[first["target"]], 1)
            (root / first_path / "result.txt").unlink()
            extra = root / "HW99/extra/release/profile-0"
            extra.mkdir(parents=True)
            (extra / "metadata.json").write_text("{}", encoding="utf-8")
            (extra / "result.txt").write_text("PASS\n", encoding="ascii")
            with self.assertRaises(run_hw.TestFailure) as caught:
                run_hw.verify_release_evidence(
                    root, combinations, targets, candidate_sha
                )
            message = str(caught.exception)
            self.assertIn("missing result:", message)
            self.assertIn("extra combination metadata:", message)
            self.assertIn("extra result:", message)

    def test_release_evidence_verifier_rejects_candidate_sha_mismatch(self):
        targets, combinations = self.release_matrix()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.write_release_evidence(root, combinations, targets, "c" * 40)
            with self.assertRaisesRegex(run_hw.TestFailure, "library_sha expected"):
                run_hw.verify_release_evidence(
                    root, combinations, targets, "d" * 40
                )

    def test_release_suite_dispatches_and_verifies_all_runs(self):
        with tempfile.TemporaryDirectory() as directory, mock.patch.object(
            sys,
            "argv",
            [
                "run_hw.py",
                "--release-suite",
                "--yes",
                "--mcu-order",
                "STM32H7B0VB",
                "--mcu-order",
                "STM32F042G6",
                "--evidence-dir",
                directory,
            ],
        ), mock.patch.object(
            run_hw, "source_identity", return_value=("e" * 40, False)
        ), mock.patch.object(
            run_hw, "run_target"
        ) as run_target, mock.patch.object(
            run_hw, "verify_release_evidence", return_value=182
        ) as verify, mock.patch(
            "builtins.print"
        ):
            self.assertEqual(run_hw.main(), 0)
            self.assertEqual(run_target.call_count, 182)
            self.assertEqual(run_target.call_args_list[0][0][0], "h7b0_make")
            self.assertEqual(run_target.call_args_list[23][0][0], "h7b0_cmake")
            self.assertEqual(run_target.call_args_list[46][0][0], "f042_make")
            verify.assert_called_once()

    def test_release_suite_rejects_dirty_candidate_before_dispatch(self):
        with mock.patch.object(
            sys, "argv", ["run_hw.py", "--release-suite", "--yes"]
        ), mock.patch.object(
            run_hw, "source_identity", return_value=("f" * 40 + "-dirty", True)
        ), mock.patch.object(
            run_hw, "run_target"
        ) as run_target, mock.patch(
            "builtins.print"
        ):
            self.assertEqual(run_hw.main(), 1)
            run_target.assert_not_called()

    def test_build_only_does_not_require_runtime_gate(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            project = root / "project"
            project.mkdir()
            toolchain = root / "toolchain"
            toolchain.mkdir()
            target = {
                "project_dir": str(project),
                "build": {"debug": []},
                "build_system": "make",
                "toolchain_bin": str(toolchain),
                "mcu": "STM32H7B0VB",
                "core": "Cortex-M7",
                "fixture": "unused",
                "artifacts": ["unused"],
                "hw08_gate_symbol": "HW08_HostGate",
            }
            with mock.patch.object(run_hw, "check_fixture"), mock.patch.object(
                run_hw,
                "validate_project_library",
                return_value=(project / "ARM_SEGGER_RTT", "candidate", False),
            ), mock.patch.object(
                run_hw, "run_commands"
            ), mock.patch.object(run_hw, "archive_artifacts"), mock.patch.object(
                run_hw, "find_elf", return_value=project / "firmware.elf"
            ), mock.patch.object(
                run_hw, "locate_symbol", return_value=0x08000000
            ) as locate_symbol:
                run_hw.run_target(
                    "h7b0_make",
                    target,
                    "debug",
                    root / "evidence",
                    1,
                    None,
                    False,
                    True,
                    1,
                    False,
                    "HW08",
                    0,
                    256,
                    0,
                    0,
                    False,
                    "candidate",
                    False,
                )
            self.assertEqual(locate_symbol.call_count, 1)
            result = (
                root
                / "evidence/HW08/h7b0_make/debug/float-fast-0_skip-asm-0/profile-0/result.txt"
            )
            self.assertEqual(result.read_text(encoding="ascii"), "BUILD PASS\n")


if __name__ == "__main__":
    unittest.main()
