#!/usr/bin/env python3

from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import run_nh


class NHRunnerTests(unittest.TestCase):
    def make_workspace(self, root: Path) -> dict[Path, tuple[Path, str, bool]]:
        identities = {}
        for name in run_nh.WORKSPACE_PROJECTS:
            library = root / name / "ARM_SEGGER_RTT"
            library.mkdir(parents=True)
            resolved = library.resolve()
            identities[resolved] = (resolved, "candidate", False)
        return identities

    def test_workspace_candidate_accepts_matching_clean_libraries(self):
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            identities = self.make_workspace(workspace)
            with mock.patch.object(
                run_nh, "git_identity", side_effect=lambda path: identities[path.resolve()]
            ):
                result = run_nh.validate_workspace_candidate(workspace, "candidate")
        self.assertEqual(len(result), len(run_nh.WORKSPACE_PROJECTS))
        self.assertTrue(all(not identity["dirty"] for identity in result))

    def test_workspace_candidate_rejects_revision_mismatch(self):
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            identities = self.make_workspace(workspace)
            first = next(iter(identities))
            identities[first] = (first, "old", False)
            with mock.patch.object(
                run_nh, "git_identity", side_effect=lambda path: identities[path.resolve()]
            ):
                with self.assertRaisesRegex(run_nh.NHFailure, "revision mismatch"):
                    run_nh.validate_workspace_candidate(workspace, "candidate")

    def test_workspace_candidate_rejects_dirty_library(self):
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            identities = self.make_workspace(workspace)
            first = next(iter(identities))
            identities[first] = (first, "candidate", True)
            with mock.patch.object(
                run_nh, "git_identity", side_effect=lambda path: identities[path.resolve()]
            ):
                with self.assertRaisesRegex(run_nh.NHFailure, "worktree is dirty"):
                    run_nh.validate_workspace_candidate(workspace, "candidate")

    def test_run_case_records_candidate_identity(self):
        process = mock.Mock(stdout=["NH-02: PASS\n"])
        process.wait.return_value = 0
        with tempfile.TemporaryDirectory() as directory, mock.patch.object(
            run_nh, "preflight", return_value={"host_cc": "cc version"}
        ), mock.patch.object(
            run_nh, "repository_identity", return_value=("a" * 40, True)
        ), mock.patch.object(
            run_nh.subprocess, "Popen", return_value=process
        ):
            evidence = Path(directory) / "evidence"
            self.assertTrue(
                run_nh.run_case(
                    "NH02", run_nh.defaults(), evidence, False, "a" * 40, True
                )
            )
            metadata = json.loads(
                (evidence / "NH02/metadata.json").read_text(encoding="utf-8")
            )
        self.assertEqual(metadata["repository_sha"], "a" * 40)
        self.assertTrue(metadata["source_dirty"])

    def test_preflight_failure_still_records_candidate_identity(self):
        with tempfile.TemporaryDirectory() as directory, mock.patch.object(
            run_nh, "preflight", side_effect=run_nh.NHFailure("missing tool")
        ), mock.patch.object(
            run_nh, "repository_identity", return_value=("b" * 40, False)
        ):
            evidence = Path(directory) / "evidence"
            with self.assertRaisesRegex(run_nh.NHFailure, "missing tool"):
                run_nh.run_case(
                    "NH02", run_nh.defaults(), evidence, False, "b" * 40, False
                )
            metadata = json.loads(
                (evidence / "NH02/metadata.json").read_text(encoding="utf-8")
            )
        self.assertEqual(metadata["repository_sha"], "b" * 40)
        self.assertFalse(metadata["source_dirty"])
        self.assertEqual(metadata["preflight_error"], "missing tool")

    def test_all_rejects_dirty_candidate_before_creating_evidence(self):
        with tempfile.TemporaryDirectory() as directory, mock.patch.object(
            sys,
            "argv",
            [
                "run_nh.py",
                "--all",
                "--evidence-dir",
                str(Path(directory) / "evidence"),
            ],
        ), mock.patch.object(
            run_nh, "repository_identity", return_value=("c" * 40, True)
        ), mock.patch.object(
            run_nh, "run_case"
        ) as run_case:
            with self.assertRaisesRegex(
                run_nh.NHFailure, "requires a clean committed candidate"
            ):
                run_nh.main()
            run_case.assert_not_called()
            self.assertFalse((Path(directory) / "evidence").exists())

    def test_repository_identity_change_is_rejected(self):
        with mock.patch.object(
            run_nh,
            "repository_identity",
            return_value=("d" * 40, True),
        ):
            with self.assertRaisesRegex(run_nh.NHFailure, "candidate changed"):
                run_nh.require_repository_identity("d" * 40, False)


if __name__ == "__main__":
    unittest.main()
