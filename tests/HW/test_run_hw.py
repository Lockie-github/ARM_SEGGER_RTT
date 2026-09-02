#!/usr/bin/env python3

import tempfile
import unittest
from pathlib import Path
from unittest import mock

import run_hw


class HWRunnerTests(unittest.TestCase):
    def test_resource_profiles_select_expected_implementations(self):
        expected = {
            1: (0, 0),
            2: (0, 0),
            3: (0, 0),
            4: (1, 0),
            5: (0, 0),
            6: (0, 1),
        }
        for profile, options in expected.items():
            with self.subTest(profile=profile):
                self.assertEqual(run_hw.resolve_hw08_options(profile, 0, 0), options)

    def test_resource_profiles_reject_conflicting_overrides(self):
        with self.assertRaises(run_hw.TestFailure):
            run_hw.resolve_hw08_options(4, 1, 0)
        with self.assertRaises(run_hw.TestFailure):
            run_hw.resolve_hw08_options(6, 0, 1)

    def test_evidence_variants_include_matrix_parameters(self):
        self.assertEqual(
            run_hw.evidence_variant("HW06", 0, 128, 0, 0, 0, "marker", False),
            Path("up-size-128/profile-0"),
        )
        self.assertEqual(
            run_hw.evidence_variant("HW08", 0, 256, 1, 1, 0, "marker", False),
            Path("float-fast-1_skip-asm-1/profile-0"),
        )
        self.assertEqual(
            run_hw.evidence_variant("HW08", 0, 256, 1, 0, 4, "marker", False),
            Path("resource-profile-4/profile-0"),
        )
        self.assertEqual(
            run_hw.evidence_variant(
                "HW08", 0, 256, 0, 0, 0, "gated-throughput", True
            ),
            Path("qualification"),
        )

    def test_matrix_evidence_variants_are_unique(self):
        hw06 = {
            run_hw.evidence_variant("HW06", 0, size, 0, 0, 0, "marker", False)
            for size in (128, 256)
        }
        markers = {
            run_hw.evidence_variant("HW08", 0, 256, fast, asm, 0, "marker", False)
            for fast in (0, 1)
            for asm in (0, 1)
        }
        resources = {
            run_hw.evidence_variant("HW08", 0, 256, 0, 0, profile, "marker", False)
            for profile in range(1, 7)
        }
        self.assertEqual(len(hw06), 2)
        self.assertEqual(len(markers), 4)
        self.assertEqual(len(resources), 6)
        self.assertTrue(markers.isdisjoint(resources))

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
                    1,
                    False,
                    "candidate",
                    False,
                )
            self.assertEqual(locate_symbol.call_count, 1)
            result = (
                root
                / "evidence/HW08/h7b0_make/debug/resource-profile-1/profile-0/result.txt"
            )
            self.assertEqual(result.read_text(encoding="ascii"), "BUILD PASS\n")


if __name__ == "__main__":
    unittest.main()
