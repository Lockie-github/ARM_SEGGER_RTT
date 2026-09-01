#!/usr/bin/env python3
"""Run the shared NH01-NH12 non-hardware test suite."""

from __future__ import annotations

import argparse
import datetime as dt
import json
import os
from pathlib import Path
import platform
import shlex
import shutil
import subprocess
import sys
from typing import Any


HERE = Path(__file__).resolve().parent
REPO_ROOT = HERE.parents[1]
DEFAULT_LOCAL_CONFIG = HERE / "config.local.json"
CASES: dict[str, dict[str, Any]] = {
    "NH01": {"script": "tests/NH/cases/NH01/run.sh", "needs": ["host", "arm"]},
    "NH02": {"script": "tests/NH/cases/NH02/run.sh", "needs": ["host"]},
    "NH03": {"script": "tests/NH/cases/NH03/run.sh", "needs": ["host"]},
    "NH04": {"script": "tests/NH/cases/NH04/run.sh", "needs": ["host", "git", "tar"]},
    "NH05": {"script": "tests/NH/cases/NH05/run.sh", "needs": ["host", "host_nm"]},
    "NH06": {"script": "tests/NH/cases/NH06/run.sh", "needs": ["host", "arm"]},
    "NH07": {
        "script": "tests/NH/cases/NH07/run.sh",
        "needs": ["host", "arm", "make", "cmake", "ninja"],
    },
    "NH08": {"script": "tests/NH/cases/NH08/run.sh", "needs": ["host"]},
    "NH09": {
        "script": "tests/NH/cases/NH09/run.sh",
        "needs": ["arm", "make", "cmake", "cube", "ninja", "git", "workspace"],
    },
    "NH10": {
        "script": "tests/NH/cases/NH10/run.sh",
        "needs": ["arm", "make", "cmake", "cube", "ninja", "git", "workspace"],
    },
    "NH11": {
        "script": "tests/NH/cases/NH11/run.sh",
        "needs": ["host", "host_nm", "arm"],
    },
    "NH12": {"script": "tests/NH/cases/NH12/run.sh", "needs": ["host"]},
}


class NHFailure(RuntimeError):
    pass


def defaults() -> dict[str, Any]:
    return {
        "schema_version": 1,
        "shell": "sh",
        "host_cc": "cc",
        "host_nm": "nm",
        "toolchain_prefix": "arm-none-eabi-",
        "cube_cmake": "cube-cmake",
        "cube": "cube",
        "workspace": str(REPO_ROOT.parent),
        "jobs": 8,
        "environment": {},
        "cases": {},
    }


def load_config(path: Path | None) -> dict[str, Any]:
    config = defaults()
    if path is None and DEFAULT_LOCAL_CONFIG.is_file():
        path = DEFAULT_LOCAL_CONFIG
    if path is not None:
        with path.open(encoding="utf-8") as stream:
            user_config = json.load(stream)
        if user_config.get("schema_version", 1) != 1:
            raise NHFailure(f"unsupported config schema: {user_config.get('schema_version')!r}")
        for key, value in user_config.items():
            if key in ("environment", "cases"):
                config[key] = {**config[key], **value}
            else:
                config[key] = value
    return config


def resolve_path(value: str, base: Path) -> Path:
    path = Path(value).expanduser()
    return (base / path).resolve() if not path.is_absolute() else path.resolve()


def shell_path(shell: str, path: Path) -> str:
    if os.name != "nt":
        return str(path)
    result = subprocess.run(
        [shell, "-lc", f"cygpath -u {shlex.quote(str(path))}"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        raise NHFailure("Windows execution requires Git Bash with cygpath")
    return result.stdout.strip()


def shell_command(shell: str, value: str, suffix: str = "") -> str:
    """Convert configured executable paths for the selected POSIX shell."""
    candidate = Path(value + suffix).expanduser()
    if os.name != "nt" or candidate.parent == Path("."):
        return value
    if not candidate.is_file():
        return value
    converted = shell_path(shell, candidate.resolve())
    return converted[: -len(suffix)] if suffix else converted


def command_available(shell: str, command: str, environment: dict[str, str]) -> bool:
    result = subprocess.run(
        [shell, "-lc", f"command -v {shlex.quote(command)} >/dev/null 2>&1"],
        cwd=REPO_ROOT,
        env=environment,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    return result.returncode == 0


def build_environment(
    config: dict[str, Any], case: str, case_dir: Path
) -> dict[str, str]:
    shell = str(config["shell"])
    environment = os.environ.copy()
    injected_paths = [shell_path(shell, HERE / "support")]
    for configured_tool in (str(config["cube_cmake"]), str(config["cube"])):
        tool_path = Path(configured_tool).expanduser()
        if tool_path.parent != Path("."):
            injected_paths.append(shell_path(shell, tool_path.resolve().parent))
    environment["PATH"] = os.pathsep.join(injected_paths) + os.pathsep + environment.get("PATH", "")
    host_cc = shell_command(shell, str(config["host_cc"]))
    host_nm = shell_command(shell, str(config["host_nm"]))
    toolchain_prefix = shell_command(
        shell, str(config["toolchain_prefix"]), suffix="gcc"
    )
    cube_cmake = shell_command(shell, str(config["cube_cmake"]))
    cube = shell_command(shell, str(config["cube"]))
    environment.update(
        {
            "CC": host_cc,
            "HOST_CC": host_cc,
            "HOST_NM": host_nm,
            "TOOLCHAIN_PREFIX": toolchain_prefix,
            "CUBE_CMAKE": cube_cmake,
            "CUBE": cube,
            "NH_WORKSPACE": shell_path(
                shell, resolve_path(str(config["workspace"]), REPO_ROOT)
            ),
            "NH_JOBS": str(int(config["jobs"])),
            "TMPDIR": shell_path(shell, case_dir / "tmp"),
        }
    )
    output = shell_path(shell, case_dir / "artifacts")
    environment["NH_OUTPUT_DIR"] = output
    environment["NH11_OUTPUT_DIR"] = output
    environment["NH12_OUTPUT_DIR"] = output
    environment["NH09_EVIDENCE_DIR"] = output
    environment["NH10_EVIDENCE_DIR"] = output
    environment.update({str(key): str(value) for key, value in config["environment"].items()})
    case_config = config["cases"].get(case, {})
    environment.update(
        {str(key): str(value) for key, value in case_config.get("environment", {}).items()}
    )
    return environment


def preflight(
    case: str, config: dict[str, Any], environment: dict[str, str]
) -> dict[str, str]:
    shell = str(config["shell"])
    if shutil.which(shell) is None and not Path(shell).is_file():
        raise NHFailure(f"POSIX shell was not found: {shell}")

    needs = set(CASES[case]["needs"])
    commands = {"awk", "cmp", "find", "grep", "sed"}
    if "host" in needs:
        commands.add(environment["HOST_CC"])
    if "host_nm" in needs:
        commands.add(environment["HOST_NM"])
    if "arm" in needs:
        prefix = environment["TOOLCHAIN_PREFIX"]
        commands.update(prefix + tool for tool in ("gcc", "nm", "objdump", "size"))
    if "make" in needs:
        commands.add("make")
    if "cmake" in needs:
        commands.add(environment["CUBE_CMAKE"])
    if "cube" in needs:
        commands.add(environment["CUBE"])
    if "ninja" in needs:
        commands.add("ninja")
    if "git" in needs:
        commands.add("git")
    if "tar" in needs:
        commands.add("tar")

    missing = sorted(
        command for command in commands if not command_available(shell, command, environment)
    )
    if missing:
        raise NHFailure(f"{case} missing required commands: {', '.join(missing)}")

    if "arm" in needs:
        arm_gcc = environment["TOOLCHAIN_PREFIX"] + "gcc"
        header_probe = subprocess.run(
            [
                shell,
                "-lc",
                "printf '#include <stdlib.h>\\n' | "
                + shlex.quote(arm_gcc)
                + " -xc -E - >/dev/null 2>&1",
            ],
            cwd=REPO_ROOT,
            env=environment,
            check=False,
        )
        if header_probe.returncode != 0:
            raise NHFailure(
                f"{case} Arm toolchain cannot compile <stdlib.h>: {arm_gcc}; "
                "install the Arm GNU C library headers or select a complete toolchain"
            )

    workspace = resolve_path(str(config["workspace"]), REPO_ROOT)
    if "workspace" in needs:
        projects = (
            "stm32f042g6make", "stm32f042g6cmake",
            "stm32f103c8make", "stm32f103c8cmake",
            "stm32f411cemake", "stm32f411cecmake",
            "stm32h7b0vbmake", "stm32h7b0vbcmake",
        )
        missing_projects = [name for name in projects if not (workspace / name).is_dir()]
        if missing_projects:
            raise NHFailure(
                f"{case} workspace is missing projects: {', '.join(missing_projects)}"
            )
        active_hw_files: list[str] = []
        for name in projects:
            project = workspace / name
            candidates = [project / "rtt_cfg.h"]
            candidates.extend((project / "Core" / "Src").glob("*.c"))
            for candidate in candidates:
                if not candidate.is_file():
                    continue
                try:
                    contents = candidate.read_text(encoding="utf-8", errors="ignore")
                except OSError:
                    continue
                normalized_contents = contents.replace("\\", "/")
                if any(
                    path in normalized_contents
                    for path in ("tests/HW/", "tests/hw/")
                ):
                    active_hw_files.append(str(candidate.relative_to(workspace)))
        if active_hw_files:
            display = ", ".join(active_hw_files[:6])
            if len(active_hw_files) > 6:
                display += f", and {len(active_hw_files) - 6} more"
            raise NHFailure(
                f"{case} workspace has active HW fixture injection: {display}; "
                "finish the HW local run or select a clean NH workspace"
            )

    versions: dict[str, str] = {}
    for label, command in (
        ("shell", shell),
        ("host_cc", environment["HOST_CC"]),
        ("arm_gcc", environment["TOOLCHAIN_PREFIX"] + "gcc"),
        ("cube_cmake", environment["CUBE_CMAKE"]),
    ):
        if command_available(shell, command, environment):
            result = subprocess.run(
                [shell, "-lc", f"{shlex.quote(command)} --version 2>&1 | sed -n '1p'"],
                cwd=REPO_ROOT,
                env=environment,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                check=False,
            )
            versions[label] = result.stdout.strip()
    return versions


def run_case(
    case: str,
    config: dict[str, Any],
    evidence_root: Path,
    dry_run: bool,
) -> bool:
    case_dir = evidence_root / case
    if case_dir.exists() and not dry_run:
        raise NHFailure(f"refusing to overwrite evidence: {case_dir}")
    script = REPO_ROOT / CASES[case]["script"]
    if not script.is_file():
        raise NHFailure(f"test script does not exist: {script}")

    if dry_run:
        temporary_case_dir = evidence_root / case
        environment = build_environment(config, case, temporary_case_dir)
        print(f"{case}: {config['shell']} {CASES[case]['script']}")
        print(
            "  "
            + " ".join(
                f"{name}={environment[name]}"
                for name in ("HOST_CC", "TOOLCHAIN_PREFIX", "CUBE_CMAKE", "NH_WORKSPACE")
            )
        )
        return True

    environment = build_environment(config, case, case_dir)
    started = dt.datetime.now().astimezone()
    try:
        versions = preflight(case, config, environment)
    except NHFailure as error:
        case_dir.mkdir(parents=True)
        (case_dir / "result.txt").write_text("FAIL\n", encoding="ascii")
        (case_dir / "preflight-error.txt").write_text(
            str(error) + "\n", encoding="utf-8"
        )
        raise
    case_dir.mkdir(parents=True)
    (case_dir / "tmp").mkdir()
    metadata = {
        "case": case,
        "script": CASES[case]["script"],
        "started_at": started.isoformat(),
        "platform": platform.platform(),
        "python": sys.version.split()[0],
        "shell": config["shell"],
        "host_cc": config["host_cc"],
        "host_nm": config["host_nm"],
        "toolchain_prefix": config["toolchain_prefix"],
        "cube_cmake": config["cube_cmake"],
        "workspace": str(resolve_path(str(config["workspace"]), REPO_ROOT)),
        "jobs": config["jobs"],
        "versions": versions,
    }
    (case_dir / "metadata.json").write_text(
        json.dumps(metadata, indent=2, ensure_ascii=True) + "\n", encoding="utf-8"
    )

    command = [str(config["shell"]), CASES[case]["script"]]
    print(f"==> {case}", flush=True)
    print("$ " + " ".join(shlex.quote(part) for part in command), flush=True)
    with (case_dir / "run.log").open("w", encoding="utf-8") as log:
        process = subprocess.Popen(
            command,
            cwd=REPO_ROOT,
            env=environment,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            errors="replace",
        )
        assert process.stdout is not None
        for line in process.stdout:
            sys.stdout.write(line)
            sys.stdout.flush()
            log.write(line)
        return_code = process.wait()

    metadata["finished_at"] = dt.datetime.now().astimezone().isoformat()
    metadata["return_code"] = return_code
    (case_dir / "metadata.json").write_text(
        json.dumps(metadata, indent=2, ensure_ascii=True) + "\n", encoding="utf-8"
    )
    result = "PASS" if return_code == 0 else "FAIL"
    (case_dir / "result.txt").write_text(result + "\n", encoding="ascii")
    return return_code == 0


def write_summary(evidence_root: Path, results: dict[str, bool]) -> None:
    lines = [
        "# NH Test Summary",
        "",
        f"- Generated: {dt.datetime.now().astimezone().isoformat()}",
        f"- Repository: {REPO_ROOT}",
        "",
        "| Case | Result |",
        "|---|---|",
    ]
    lines.extend(
        f"| {case} | {'PASS' if passed else 'FAIL'} |"
        for case, passed in results.items()
    )
    lines.append("")
    (evidence_root / "SUMMARY.md").write_text("\n".join(lines), encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument("--case", action="append", choices=tuple(CASES))
    selection.add_argument("--all", action="store_true")
    parser.add_argument("--list", action="store_true")
    parser.add_argument("--config", type=Path)
    parser.add_argument("--evidence-dir", type=Path)
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--keep-going", action="store_true")
    parser.add_argument("--shell")
    parser.add_argument("--host-cc")
    parser.add_argument("--host-nm")
    parser.add_argument("--toolchain-prefix")
    parser.add_argument("--cube-cmake")
    parser.add_argument("--cube")
    parser.add_argument("--workspace")
    parser.add_argument("--jobs", type=int)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.list:
        for case, entry in CASES.items():
            print(f"{case}  {entry['script']}  [{', '.join(entry['needs'])}]")
        return 0
    selected = list(CASES) if args.all else (args.case or [])
    if not selected:
        raise NHFailure("select --case NHxx or --all")

    config_path = args.config.resolve() if args.config else None
    config = load_config(config_path)
    for key in (
        "shell", "host_cc", "host_nm", "toolchain_prefix", "cube_cmake",
        "cube", "workspace", "jobs",
    ):
        value = getattr(args, key)
        if value is not None:
            config[key] = value
    if int(config["jobs"]) < 1:
        raise NHFailure("jobs must be at least 1")

    timestamp = dt.datetime.now().astimezone().strftime("%Y%m%d_%H%M%S")
    evidence_root = (
        args.evidence_dir.resolve()
        if args.evidence_dir
        else REPO_ROOT / "TEST_EVIDENCE" / f"NH_RUN_{timestamp}"
    )
    if not args.dry_run:
        evidence_root.mkdir(parents=True, exist_ok=False)

    results: dict[str, bool] = {}
    for case in selected:
        try:
            passed = run_case(case, config, evidence_root, args.dry_run)
        except NHFailure as error:
            print(f"NH test failed: {error}", file=sys.stderr)
            passed = False
        results[case] = passed
        if not passed and not args.keep_going:
            break
    if not args.dry_run:
        write_summary(evidence_root, results)
        print(f"evidence: {evidence_root}")
    return 0 if results and all(results.values()) else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except NHFailure as error:
        print(f"NH test failed: {error}", file=sys.stderr)
        raise SystemExit(1)
