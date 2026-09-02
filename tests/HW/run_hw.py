#!/usr/bin/env python3
"""Build, flash, capture and verify the shared STM32 HW test matrix."""

from __future__ import annotations

import argparse
import collections
import datetime as dt
import glob
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import secrets
import struct
import subprocess
import sys
import time
from typing import Any
import zlib


HERE = Path(__file__).resolve().parent
REPO_ROOT = HERE.parents[1]
DEFAULT_CONFIG = HERE / "targets.json"
CASES = tuple(f"HW{number:02d}" for number in range(1, 9))
JLINK_FAILURE_PATTERNS = (
    "Cannot connect to J-Link",
    "Cannot connect to target",
    "No J-Link device found",
    "No emulator connected",
)


class TestFailure(RuntimeError):
    pass


def source_identity() -> tuple[str, bool]:
    revision = subprocess.run(
        ["git", "-C", str(REPO_ROOT), "rev-parse", "HEAD"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )
    if revision.returncode != 0:
        raise TestFailure("cannot determine the ARM_SEGGER_RTT Git revision")
    status = subprocess.run(
        ["git", "-C", str(REPO_ROOT), "status", "--porcelain", "--untracked-files=normal"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )
    if status.returncode != 0:
        raise TestFailure("cannot determine the ARM_SEGGER_RTT worktree state")
    dirty = bool(status.stdout.strip())
    identity = revision.stdout.strip() + ("-dirty" if dirty else "")
    return identity, dirty


def load_config(path: Path) -> tuple[dict[str, Any], dict[str, dict[str, Any]]]:
    with path.open(encoding="utf-8") as stream:
        document = json.load(stream)
    if document.get("schema_version") != 1:
        raise TestFailure(f"unsupported config schema: {document.get('schema_version')!r}")
    defaults = document.get("defaults", {})
    targets = document.get("targets", {})
    if not targets:
        raise TestFailure("configuration has no targets")
    return defaults, targets


def merged_target(defaults: dict[str, Any], target: dict[str, Any]) -> dict[str, Any]:
    return {**defaults, **target}


def locate_cube_cmake() -> Path:
    configured = os.environ.get("CUBE_CMAKE")
    candidates = [Path(configured).expanduser()] if configured else []
    command = shutil.which("cube-cmake")
    if command:
        candidates.append(Path(command))
    candidates.extend(
        Path(name)
        for name in glob.glob(
            str(
                Path.home()
                / ".vscode/extensions/stmicroelectronics.stm32cube-ide-build-cmake-*"
                / "resources/cube-cmake/*/*/cube-cmake"
            )
        )
    )
    available = sorted(
        (candidate.resolve() for candidate in candidates if candidate.is_file()),
        reverse=True,
    )
    if not available:
        raise TestFailure(
            "cube-cmake was not found; set CUBE_CMAKE to the STM32Cube extension executable"
        )
    return available[0]


def format_command(command: list[str], values: dict[str, Any]) -> list[str]:
    return [part.format_map(values) for part in command]


def command_text(command: list[str]) -> str:
    return " ".join(shlex.quote(part) for part in command)


def configured_command(
    template: list[str],
    values: dict[str, Any],
    cwd: Path,
    environment: dict[str, str],
) -> list[str]:
    command = format_command(template, values)
    if (
        environment.get("HW_TEST_MAKE_OVERLAY") == "1"
        and Path(command[0]).name == "make"
    ):
        return [
            command[0],
            "-f",
            str(cwd / "Makefile"),
            "-f",
            str(HERE / "support" / "hw_test.mk"),
            *command[1:],
        ]
    return command


def cmake_build_commands(build_type: str) -> list[list[str]]:
    configuration = build_type.capitalize()
    return [
        [
            "{cube_cmake}",
            "--fresh",
            f"-DCMAKE_BUILD_TYPE={configuration}",
            "-DCMAKE_PROJECT_INCLUDE={cmake_overlay}",
            "-DHW_TEST_REPO_ROOT={repo_root}",
            "-DHW_TEST_CASE={case_number}",
            "-DHW_TEST_PROFILE={profile}",
            "-DHW_TEST_UP_SIZE={up_size}",
            "-DHW_TEST_MCU_FAMILY={mcu_family}",
            "-DHW_TEST_IRQ_HANDLER={irq_handler}",
            "-DHW_TEST_LIBRARY_SHA={library_sha}",
            "-DHW08_FLOAT_FAST={float_fast}",
            "-DHW08_SKIP_ASM={skip_asm}",
            "-DHW08_RESOURCE_PROFILE={resource_profile}",
            "-DHW08_GATED_THROUGHPUT={gated_throughput}",
            "-DHW08_TP_RUN_ID={run_id}",
            "-DHW08_TP_BASE_TICKS={tp_base_ticks}",
            "-DHW08_TP_BASE_CYCLES={tp_base_cycles}",
            "-DHW08_TP_REMAINDER_STEP={tp_remainder_step}",
            "-DHW08_TP_REMAINDER_DENOM={tp_remainder_denom}",
            "-DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake",
            "-S", ".",
            "-B", "build/{cmake_build}",
            "-G", "Ninja",
        ],
        [
            "{cube_cmake}", "--build", "build/{cmake_build}",
            "--target", "all", "--", "-j{jobs}",
        ],
    ]


def run_commands(
    commands: list[list[str]],
    cwd: Path,
    log_path: Path,
    values: dict[str, Any],
    environment: dict[str, str],
    dry_run: bool,
) -> None:
    if dry_run:
        for template in commands:
            command = configured_command(template, values, cwd, environment)
            print(f"$ {command_text(command)}")
        return
    with log_path.open("a", encoding="utf-8") as log:
        for template in commands:
            command = configured_command(template, values, cwd, environment)
            line = f"$ {command_text(command)}"
            print(line)
            log.write(line + "\n")
            log.flush()
            result = subprocess.run(
                command,
                cwd=cwd,
                env=environment,
                stdout=log,
                stderr=subprocess.STDOUT,
                text=True,
                check=False,
            )
            if result.returncode != 0:
                raise TestFailure(
                    f"command failed with status {result.returncode}: {command_text(command)}"
                )


def log_failure(path: Path) -> str | None:
    if not path.exists():
        return None
    text = path.read_text(encoding="utf-8", errors="replace")
    return next((pattern for pattern in JLINK_FAILURE_PATTERNS if pattern in text), None)


def require_no_log_failure(path: Path, context: str) -> None:
    failure = log_failure(path)
    if failure:
        raise TestFailure(f"{context} failed: {failure}; see {path}")


def wait_for_text(path: Path, needle: str, timeout: int, process: subprocess.Popen[Any]) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise TestFailure(f"process exited before {needle!r} appeared")
        if path.exists():
            require_no_log_failure(path, "J-Link session")
            if needle in path.read_text(encoding="utf-8", errors="replace"):
                return
        time.sleep(0.25)
    raise TestFailure(f"timed out waiting for {needle!r}")


def wait_for_text_after(
    path: Path, needle: str, offset: int, timeout: int, process: subprocess.Popen[Any]
) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise TestFailure(f"process exited before new {needle!r} appeared")
        if path.exists():
            require_no_log_failure(path, "J-Link session")
            data = path.read_bytes()[offset:].decode("utf-8", errors="replace")
            if needle in data:
                return
        time.sleep(0.25)
    raise TestFailure(f"timed out waiting for new {needle!r}")


def stop_process(process: subprocess.Popen[Any] | None, send_exit: bool = False) -> None:
    if process is None or process.poll() is not None:
        return
    if send_exit and process.stdin is not None:
        try:
            process.stdin.write("exit\n")
            process.stdin.flush()
            process.wait(timeout=5)
            return
        except (BrokenPipeError, subprocess.TimeoutExpired):
            pass
    process.terminate()
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()


def capture_rtt(
    target: dict[str, Any],
    evidence: Path,
    expected: list[str],
    probe_serial: str | None,
    reset_target: bool = True,
    rtt_channel: int = 0,
    minimum_heartbeat: int | None = None,
    start_gate: tuple[str, bytes] | None = None,
    memory_start_gate: tuple[str, int, int] | None = None,
    entry_address: int | None = None,
) -> bytes:
    jlink_log = evidence / "jlink-server.log"
    commander_log = evidence / "jlink-commander.log"
    raw_log = evidence / "rtt.transport.raw"
    client_error = evidence / "rtt-client.err"
    jlink_command = [
        "JLinkExe",
        "-Device",
        target["mcu"],
        "-if",
        "SWD",
        "-Speed",
        str(target["jlink_speed"]),
        "-RTTTelnetPort",
        str(target["rtt_port"]),
        "-autoconnect",
        "1",
        "-Log",
        str(jlink_log),
    ]
    if probe_serial:
        jlink_command.extend(["-SelectEmuBySN", probe_serial])

    commander_stream = commander_log.open("wb")
    raw_stream = raw_log.open("wb")
    error_stream = client_error.open("wb")
    jlink: subprocess.Popen[Any] | None = None
    client: subprocess.Popen[Any] | None = None
    try:
        jlink = subprocess.Popen(
            jlink_command,
            stdin=subprocess.PIPE,
            stdout=commander_stream,
            stderr=subprocess.STDOUT,
            text=True,
        )
        wait_for_text(
            commander_log,
            f"{target['core']} identified.",
            int(target["probe_ready_timeout"]),
            jlink,
        )
        if rtt_channel == 0:
            client_command = [
                "JLinkRTTClient",
                "-RTTTelnetPort",
                str(target["rtt_port"]),
                "-LocalEcho",
                "Off",
            ]
        else:
            client_command = ["nc", "127.0.0.1", str(target["rtt_port"])]

        def start_client() -> subprocess.Popen[Any]:
            process = subprocess.Popen(
                client_command,
                stdin=subprocess.PIPE if rtt_channel or start_gate is not None else None,
                stdout=raw_stream,
                stderr=error_stream,
            )
            if rtt_channel:
                assert process.stdin is not None
                process.stdin.write(
                    f"$$SEGGER_TELNET_ConfigStr=RTTCh;{rtt_channel}$$".encode("ascii")
                )
                process.stdin.flush()
            return process

        # A CMake overlay enters the fixture only after the project has initialized
        # the MCU. Delay the RTT connection until then so NOLOAD control blocks from
        # a previous case cannot be accepted as the current session.
        if entry_address is None or not reset_target:
            client = start_client()
            wait_for_text(
                jlink_log,
                "RTT CB verified",
                int(target["rtt_ready_timeout"]),
                jlink,
            )
        assert jlink.stdin is not None
        capture_offset = 0
        jlink_log_offset = 0
        if reset_target:
            raw_stream.flush()
            capture_offset = raw_log.stat().st_size if raw_log.exists() else 0
            jlink_log_offset = jlink_log.stat().st_size if jlink_log.exists() else 0
        if reset_target and entry_address is not None:
            jlink.stdin.write("r\ng\n")
            jlink.stdin.flush()
            time.sleep(float(target.get("entry_delay_seconds", 0.25)))
            jlink.stdin.write(f"h\nSetPC 0x{entry_address:08X}\ng\n")
            jlink.stdin.flush()
            time.sleep(float(target.get("entry_rtt_settle_seconds", 0.05)))
            client = start_client()
        else:
            jlink.stdin.write("r\ng\n" if reset_target else "g\n")
            jlink.stdin.flush()
        if reset_target and (
            entry_address is not None
            or start_gate is not None
            or memory_start_gate is not None
        ):
            wait_for_text_after(
                jlink_log,
                "RTT CB verified",
                jlink_log_offset,
                int(target["rtt_ready_timeout"]),
                jlink,
            )

        deadline = time.monotonic() + int(target["test_timeout"])
        gate_sent = start_gate is None and memory_start_gate is None
        while time.monotonic() < deadline:
            raw_stream.flush()
            data = raw_log.read_bytes() if raw_log.exists() else b""
            normalized = data[capture_offset:].replace(b"\r", b"")
            text = normalized.decode("utf-8", errors="replace")
            if not gate_sent:
                if start_gate is not None and start_gate[0] in text:
                    assert client.stdin is not None
                    client.stdin.write(start_gate[1])
                    client.stdin.flush()
                    gate_sent = True
                elif memory_start_gate is not None and memory_start_gate[0] in text:
                    _, address, value = memory_start_gate
                    jlink.stdin.write(
                        f"h\nw4 0x{address:08X}, 0x{value:08X}\ng\n"
                    )
                    jlink.stdin.flush()
                    gate_sent = True
            if all(pattern in text for pattern in expected):
                if minimum_heartbeat is None:
                    return normalized
                try:
                    current_heartbeat = heartbeat(normalized, target)
                except TestFailure:
                    pass
                else:
                    if current_heartbeat > minimum_heartbeat:
                        return normalized
            if client.poll() is not None:
                raise TestFailure("JLinkRTTClient exited before the expected output arrived")
            time.sleep(0.25)
        raise TestFailure("timed out waiting for the expected RTT output")
    finally:
        stop_process(client)
        stop_process(jlink, send_exit=True)
        commander_stream.close()
        raw_stream.close()
        error_stream.close()


def count_lines(text: str, prefix: str, suffix: str | None = None) -> int:
    return sum(
        1 for line in text.splitlines()
        if line.startswith(prefix) and (suffix is None or line.endswith(suffix))
    )


def verify_output(
    data: bytes,
    target: dict[str, Any],
    build_type: str,
    case: str,
    profile: int,
    library_sha: str,
) -> None:
    text = data.decode("utf-8", errors="replace")
    expected_by_case = {
        "HW02": ["HW-02|BEGIN|", "HW-02|END|"],
        "HW03": [f"HW-03|BEGIN|{target['mcu']}|", "HW-03|END|SIDE_EFFECT=PASS|"],
        "HW04": [f"HW-04|BEGIN|{target['mcu']}|", "HW-04|END|PASS=10|TOTAL=10|RESULT=PASS"],
        "HW05": [f"HW-05|BEGIN|{target['mcu']}|", "HW-05|END|CORPUS=276/276|BOUNDARY=4/4|ITERATIONS=40000|STACK=PASS|RESULT=PASS"],
        "HW06": [f"HW-06|BEGIN|{target['mcu']}|", "HW-06|END|CASES=11/11|", "|STACK=PASS|RESULT=PASS"],
        "HW07": [f"HW-07|BEGIN|{target['mcu']}|", "HW-07|END|PHASES=6/6|", "|CB=PASS|STACK=PASS|FAULT=NONE|RESULT=PASS"],
        "HW08": [f"HW-08|BEGIN|{target['mcu']}|", "HW-08|END|PATHS=6/6|", "|CB=PASS|FAULT=NONE|STACK=PASS|RESULT=PASS"],
    }
    expected = expected_by_case[case]
    expected.append(f"|SHA={library_sha}")
    begin = expected[0]
    begin_offset = text.rfind(begin)
    if begin_offset >= 0:
        text = text[begin_offset:]
    if case == "HW02":
        if target["mcu"] == "STM32F042G6":
            expected.extend(
                [
                    f"PROJECT={target['build_system'].upper()}",
                    f"BUILD={build_type.upper()}",
                ]
            )
        else:
            expected.append(
                f"|{target['build_system'].upper()}|{build_type.upper()}|"
            )
    else:
        expected.append(f"|{build_type.upper()}|")
    if case == "HW03" and profile == 6:
        expected.append("|CHANNEL1|CH=1|")
    missing = [pattern for pattern in expected if pattern not in text]
    if missing:
        raise TestFailure("missing RTT patterns: " + ", ".join(repr(item) for item in missing))
    forbidden = [pattern for pattern in target["forbidden_patterns"] if pattern in text]
    if forbidden:
        raise TestFailure("forbidden RTT patterns: " + ", ".join(repr(item) for item in forbidden))
    expected_counts = {
        "HW04": [("HW-04|CASE|", "|PASS", 10)],
        "HW05": [
            ("HW-05|CORPUS|", "|PASS", 20 if target["mcu"] == "STM32F042G6" else 276),
            ("HW-05|BOUNDARY|", "|PASS", 4),
            ("HW-05|HEARTBEAT|", "|PASS", 16),
        ],
        "HW06": [("HW-06|CASE|", "|PASS", 11)],
        "HW07": [("HW-07|PHASE|BEGIN|", None, 6), ("HW-07|PHASE|END|", "|RESULT=PASS", 6)],
        "HW08": [("HW-08|SAMPLE|", None, int(target["sample_count"]))],
    }
    for prefix, suffix, expected_count in expected_counts.get(case, []):
        actual = count_lines(text, prefix, suffix)
        if actual != expected_count:
            raise TestFailure(
                f"{case} expected {expected_count} lines matching {prefix!r}, received {actual}"
            )
    if case == "HW05" and target["mcu"] == "STM32F042G6":
        if count_lines(text, "HW-05|RANDOM|", "|PASS") != 1:
            raise TestFailure("HW05 F042 random corpus summary is missing or failed")
    if case == "HW05":
        fast = profile & 1
        path_expectations = {
            "LEGACY46": (1, 0) if fast else (0, 1),
            "LEGACY47": (0, 1),
            "TYPED46": (1, 0),
            "TYPED47": (1, 0),
        }
        for name, (direct, formatter) in path_expectations.items():
            pattern = re.compile(
                rf"^HW-05\|BOUNDARY\|{name}\|.*\|DIRECT={direct}\|"
                rf"FMT_CALL={formatter}\|FMT_WRITE={formatter}\|.*\|PASS$",
                re.MULTILINE,
            )
            if pattern.search(text) is None:
                raise TestFailure(
                    f"HW05 {name} did not use the expected instrumented path"
                )
    if case == "HW03" and f"HW03_PROFILE={profile}" in text:
        raise TestFailure("HW03 emitted an unresolved profile marker")


def verify_hw08_marker_throughput(data: bytes, target: dict[str, Any]) -> str:
    magic = target.get("hw08_frame_magic")
    if not magic:
        return ""
    frame_size = int(target["hw08_frame_size"])
    frame_count = int(target["hw08_frame_count"])
    frames: list[int] = []
    invalid_shape = 0
    invalid_crc = 0
    for line in data.splitlines():
        if not line.startswith(magic.encode("ascii")):
            continue
        if len(line) != frame_size - 1:
            invalid_shape += 1
            continue
        try:
            sequence = int(line[8:16], 16)
            stored_crc = int(line[16:24], 16)
        except ValueError:
            invalid_shape += 1
            continue
        if stored_crc != (zlib.crc32(line[:16] + line[24:]) & 0xFFFFFFFF):
            invalid_crc += 1
        frames.append(sequence)

    text = data.decode("utf-8", errors="replace")
    result = re.search(
        r"^HW-08\|THROUGHPUT\|COUNT=(\d+)\|ACCEPT=(\d+)\|BYTES=(\d+)\|"
        r"CYCLES=(\d+)\|HZ=(\d+)\|RESULT=PASS$",
        text,
        re.MULTILINE,
    )
    if result is None:
        raise TestFailure("HW08 qualified throughput result marker is missing")
    attempted, accepted, accepted_bytes, cycles, target_hz = map(int, result.groups())
    duration = cycles / target_hz if target_hz else 0.0
    rate = attempted * frame_size / duration / 1048576 if duration else 0.0
    expected_rate = float(target["hw08_expected_mib_s"])
    tolerance = float(target["hw08_rate_tolerance"])
    counts = collections.Counter(frames)
    duplicates = sum(count - 1 for count in counts.values() if count > 1)
    missing = len(set(range(frame_count)) - set(frames))
    unexpected = len(set(frames) - set(range(frame_count)))
    out_of_order = sum(
        1 for previous, current in zip(frames, frames[1:])
        if current <= previous
    )
    checks = {
        "target frequency": target_hz == int(target["hw08_target_hz"]),
        "attempted frames": attempted == frame_count,
        "target accepted": accepted == attempted,
        "accepted bytes": accepted_bytes == accepted * frame_size,
        "attempted rate": expected_rate * (1 - tolerance) <= rate <= expected_rate * (1 + tolerance),
        "host captured": len(frames) == accepted,
        "frame shape": invalid_shape == 0,
        "frame CRC": invalid_crc == 0,
        "missing sequences": missing == 0,
        "duplicate sequences": duplicates == 0,
        "unexpected sequences": unexpected == 0,
        "out of order": out_of_order == 0,
    }
    failed = [name for name, passed in checks.items() if not passed]
    analysis = "\n".join(
        [
            f"attempted_mib_s\t{rate:.9f}",
            f"captured_frames\t{len(frames)}",
            f"invalid_shape\t{invalid_shape}",
            f"invalid_crc\t{invalid_crc}",
            f"missing\t{missing}",
            f"duplicates\t{duplicates}",
            f"unexpected\t{unexpected}",
            f"out_of_order\t{out_of_order}",
        ]
        + [f"{name}\t{'PASS' if passed else 'FAIL'}" for name, passed in checks.items()]
        + [f"OVERALL\t{'PASS' if not failed else 'FAIL'}", ""]
    )
    if failed:
        raise TestFailure("HW08 qualified throughput checks failed: " + ", ".join(failed))
    return analysis


def heartbeat(data: bytes, target: dict[str, Any]) -> int:
    pattern = re.compile(
        rb"^HW-01\|" + re.escape(target["mcu"].encode("ascii"))
        + rb"\|[^|]+\|[^|]+\|SHA=[^|]+\|HEARTBEAT=([0-9]+)$",
        re.MULTILINE,
    )
    matches = pattern.findall(data)
    if not matches:
        raise TestFailure("HW01 heartbeat marker is missing")
    return int(matches[-1])


def live_sequence(data: bytes) -> int:
    matches = re.findall(rb"^HW-06\|LIVE\|SEQ=([0-9]+)\|", data, re.MULTILINE)
    if not matches:
        raise TestFailure("HW06 live marker is missing")
    return int(matches[-1])


def fixture_source(path: Path, visited: set[Path] | None = None) -> str:
    visited = visited or set()
    resolved = path.resolve()
    if resolved in visited or not resolved.is_file():
        return ""
    visited.add(resolved)
    source = resolved.read_text(encoding="utf-8", errors="replace")
    parts = [source]
    for match in re.finditer(r'^\s*#\s*include\s+"([^"]+)"', source, re.MULTILINE):
        included = (resolved.parent / match.group(1)).resolve()
        if included.is_file():
            parts.append(fixture_source(included, visited))
    return "\n".join(parts)


def check_fixture(target: dict[str, Any]) -> None:
    fixture = REPO_ROOT / target["fixture"]
    if not fixture.is_file():
        raise TestFailure(f"fixture does not exist: {fixture}")
    marker = target["fixture_marker"]
    source = fixture_source(fixture)
    if marker not in source:
        raise TestFailure(
            f"{fixture} does not match the configured HW suite "
            f"protocol marker {marker!r}"
        )


def find_elf(
    project: Path, target: dict[str, Any], build_type: str | None = None
) -> Path:
    configured_pattern = target.get("elf")
    if configured_pattern is None:
        configured_pattern = next(
            (pattern for pattern in target["artifacts"] if pattern.endswith(".elf")),
            None,
        )
    if configured_pattern is None:
        raise TestFailure("target does not configure an ELF artifact")
    pattern = configured_pattern.format(
        cmake_build=build_type.capitalize() if build_type else "{cmake_build}"
    )
    matches = [Path(name) for name in glob.glob(str(project / pattern))]
    if len(matches) != 1 or not matches[0].is_file():
        raise TestFailure(
            f"expected exactly one ELF matching {configured_pattern!r}, found {len(matches)}"
        )
    return matches[0]


def locate_symbol(
    elf: Path,
    target: dict[str, Any],
    evidence: Path,
    symbol_name: str | None = None,
) -> int:
    nm = target.get("nm") or str(Path(target["toolchain_bin"]) / "arm-none-eabi-nm")
    result = subprocess.run(
        [nm, str(elf)],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        check=False,
    )
    (evidence / "symbols.txt").write_text(result.stdout, encoding="utf-8")
    if result.returncode != 0:
        raise TestFailure(f"nm failed with status {result.returncode}")
    symbol = symbol_name or target["result_symbol"]
    for line in result.stdout.splitlines():
        fields = line.split()
        if len(fields) >= 3 and fields[-1] == symbol:
            return int(fields[0], 16)
    raise TestFailure(f"ELF does not define {symbol!r}")


def jlink_command(target: dict[str, Any], probe_serial: str | None) -> list[str]:
    command = [
        "JLinkExe",
        "-Device",
        target["mcu"],
        "-if",
        "SWD",
        "-Speed",
        str(target["jlink_speed"]),
        "-autoconnect",
        "1",
    ]
    if probe_serial:
        command.extend(["-SelectEmuBySN", probe_serial])
    return command


def read_target_result(
    target: dict[str, Any],
    evidence: Path,
    result_address: int,
    probe_serial: str | None,
) -> bytes:
    result_path = evidence / "result.bin"
    command = jlink_command(target, probe_serial)
    script = (
        f"connect\nsavebin {result_path} 0x{result_address:08X} "
        f"0x{int(target['result_size']):X}\nexit\n"
    )
    process = subprocess.run(
        command,
        input=script,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        check=False,
    )
    (evidence / "read-result.log").write_text(process.stdout, encoding="utf-8")
    if process.returncode != 0 or not result_path.is_file():
        raise TestFailure("failed to read the gated throughput result structure")
    data = result_path.read_bytes()
    if len(data) != int(target["result_size"]):
        raise TestFailure(f"expected {target['result_size']} result bytes, received {len(data)}")
    return data


def capture_gated_throughput(
    target: dict[str, Any],
    evidence: Path,
    result_address: int,
    entry_address: int,
    run_id: int,
    probe_serial: str | None,
) -> tuple[bytes, bytes]:
    jlink_log = evidence / "jlink-server.log"
    commander_log = evidence / "jlink-commander.log"
    raw_log = evidence / "rtt.transport.raw"
    client_error = evidence / "rtt-client.err"
    command = jlink_command(target, probe_serial)
    command.extend(
        [
            "-RTTTelnetPort",
            str(target["rtt_port"]),
            "-Log",
            str(jlink_log),
        ]
    )
    commander_stream = commander_log.open("wb")
    raw_stream = raw_log.open("wb")
    error_stream = client_error.open("wb")
    jlink: subprocess.Popen[Any] | None = None
    client: subprocess.Popen[Any] | None = None
    try:
        jlink = subprocess.Popen(
            command,
            stdin=subprocess.PIPE,
            stdout=commander_stream,
            stderr=subprocess.STDOUT,
            text=True,
        )
        wait_for_text(
            commander_log,
            f"{target['core']} identified.",
            int(target["probe_ready_timeout"]),
            jlink,
        )
        assert jlink.stdin is not None
        jlink.stdin.write("r\ng\n")
        jlink.stdin.flush()
        time.sleep(float(target.get("entry_delay_seconds", 0.25)))
        jlink.stdin.write(f"h\nSetPC 0x{entry_address:08X}\ng\n")
        jlink.stdin.flush()
        time.sleep(float(target.get("entry_rtt_settle_seconds", 0.05)))
        client = subprocess.Popen(
            [
                "JLinkRTTClient",
                "-RTTTelnetPort",
                str(target["rtt_port"]),
                "-LocalEcho",
                "Off",
            ],
            stdout=raw_stream,
            stderr=error_stream,
        )
        wait_for_text(
            jlink_log,
            "RTT CB verified",
            int(target["rtt_ready_timeout"]),
            jlink,
        )
        time.sleep(1)
        jlink.stdin.write(f"h\nw4 0x{result_address:08X}, 0x{run_id:08X}\ng\n")
        jlink.stdin.flush()
        deadline = time.monotonic() + int(target["capture_seconds"])
        while time.monotonic() < deadline:
            if jlink.poll() is not None:
                raise TestFailure("JLinkExe exited during gated throughput capture")
            if client.poll() is not None:
                raise TestFailure("JLinkRTTClient exited during gated throughput capture")
            time.sleep(0.25)
    finally:
        stop_process(client)
        stop_process(jlink, send_exit=True)
        commander_stream.close()
        raw_stream.close()
        error_stream.close()
    capture = raw_log.read_bytes()
    result = read_target_result(target, evidence, result_address, probe_serial)
    return capture, result


RESULT_FIELDS = (
    "magic", "version", "run_id", "target_hz", "frame_size", "attempted",
    "accepted", "rejected", "accepted_bytes", "cycles", "start_rd",
    "start_wr", "end_rd", "end_wr", "reset_flags", "guard",
)


def analyze_gated_throughput(
    result_data: bytes,
    capture_data: bytes,
    target: dict[str, Any],
    expected_run_id: int,
) -> str:
    values = dict(zip(RESULT_FIELDS, struct.unpack("<16I", result_data)))
    frame_magic = target["frame_magic"].encode("ascii")
    frames: list[int] = []
    invalid_crc = 0
    invalid_shape = 0
    foreign_run_frames = 0
    for line in capture_data.splitlines():
        if not line.startswith(frame_magic):
            continue
        try:
            frame_run_id = int(line[8:16], 16)
        except ValueError:
            invalid_shape += 1
            continue
        if frame_run_id != expected_run_id:
            foreign_run_frames += 1
            continue
        if len(line) != int(target["frame_size"]) - 1:
            invalid_shape += 1
            continue
        try:
            sequence = int(line[16:24], 16)
            stored_crc = int(line[24:32], 16)
        except ValueError:
            invalid_shape += 1
            continue
        actual_crc = zlib.crc32(line[:24] + line[32:]) & 0xFFFFFFFF
        if stored_crc != actual_crc:
            invalid_crc += 1
        frames.append(sequence)

    counts = collections.Counter(frames)
    duplicates = sum(count - 1 for count in counts.values() if count > 1)
    expected_sequences = set(range(int(target["frame_count"])))
    observed_sequences = set(frames)
    missing = len(expected_sequences - observed_sequences)
    unexpected = len(observed_sequences - expected_sequences)
    out_of_order = sum(
        1 for previous, current in zip(frames, frames[1:]) if current <= previous
    )
    duration = values["cycles"] / values["target_hz"] if values["target_hz"] else 0
    attempted_rate = (
        values["attempted"] * values["frame_size"] / duration / 1048576
        if duration else 0
    )
    expected_rate = float(target["expected_mib_s"])
    tolerance = float(target["rate_tolerance"])

    checks = [
        ("result magic", values["magic"] == int(target["result_magic"])),
        ("result guard", values["guard"] == int(target["result_guard"])),
        ("result version", values["version"] == int(target["result_version"])),
        ("run id", values["run_id"] == expected_run_id),
        ("target frequency", values["target_hz"] == int(target["target_hz"])),
        ("frame size", values["frame_size"] == int(target["frame_size"])),
        ("attempted frames", values["attempted"] == int(target["frame_count"])),
        ("attempted rate", expected_rate * (1 - tolerance) <= attempted_rate <= expected_rate * (1 + tolerance)),
        ("target accepted", values["accepted"] == values["attempted"]),
        ("target rejected", values["rejected"] == 0),
        ("accepted bytes", values["accepted_bytes"] == values["accepted"] * values["frame_size"]),
        ("drained offsets", values["end_rd"] == values["end_wr"]),
        ("host captured", len(frames) == values["accepted"]),
        ("frame shape", invalid_shape == 0),
        ("frame CRC", invalid_crc == 0),
        ("missing sequences", missing == values["rejected"]),
        ("duplicate sequences", duplicates == 0),
        ("unexpected sequences", unexpected == 0),
        ("out of order", out_of_order == 0),
    ]
    lines = ["metric\tvalue"]
    lines.extend(f"target_{name}\t{value}" for name, value in values.items())
    lines.extend(
        [
            f"duration_s\t{duration:.9f}",
            f"attempted_mib_s\t{attempted_rate:.9f}",
            f"foreign_run_frames\t{foreign_run_frames}",
            f"captured_frames\t{len(frames)}",
            f"invalid_shape\t{invalid_shape}",
            f"invalid_crc\t{invalid_crc}",
            f"missing\t{missing}",
            f"duplicates\t{duplicates}",
            f"unexpected\t{unexpected}",
            f"out_of_order\t{out_of_order}",
            "checks",
        ]
    )
    lines.extend(f"{'PASS' if passed else 'FAIL'}\t{name}" for name, passed in checks)
    lines.append(f"OVERALL\t{'PASS' if all(passed for _, passed in checks) else 'FAIL'}")
    return "\n".join(lines) + "\n"


def archive_artifacts(project: Path, target: dict[str, Any], evidence: Path, values: dict[str, Any]) -> None:
    output = evidence / "artifacts"
    output.mkdir()
    copied = 0
    for pattern in target["artifacts"]:
        expanded = pattern.format_map(values)
        for name in glob.glob(str(project / expanded)):
            source = Path(name)
            if source.is_file():
                shutil.copy2(source, output / source.name)
                copied += 1
    if copied == 0:
        raise TestFailure("build produced no configured artifacts")


def resolve_hw08_options(
    resource_profile: int, float_fast: int, skip_asm: int
) -> tuple[int, int]:
    if resource_profile == 0:
        return float_fast, skip_asm
    if float_fast or skip_asm:
        raise TestFailure(
            "HW08 resource profiles select their own float/Skip implementation"
        )
    return (1 if resource_profile == 4 else 0, 1 if resource_profile == 6 else 0)


def evidence_variant(
    case: str,
    profile: int,
    up_size: int,
    float_fast: int,
    skip_asm: int,
    resource_profile: int,
    protocol: str,
    throughput_qualification: bool,
) -> Path:
    if throughput_qualification:
        return Path("qualification")
    profile_leaf = Path(f"profile-{profile}")
    if case == "HW06":
        return Path(f"up-size-{up_size}") / profile_leaf
    if case == "HW08" and resource_profile:
        return Path(f"resource-profile-{resource_profile}") / profile_leaf
    if case == "HW08" and protocol == "marker":
        return Path(f"float-fast-{float_fast}_skip-asm-{skip_asm}") / profile_leaf
    return profile_leaf


def run_target(
    name: str,
    target: dict[str, Any],
    build_type: str,
    evidence_root: Path,
    jobs: int,
    probe_serial: str | None,
    dry_run: bool,
    build_only: bool,
    repetition: int,
    repeated: bool,
    case: str,
    profile: int,
    up_size: int,
    float_fast: int,
    skip_asm: int,
    resource_profile: int,
    throughput_qualification: bool,
    library_sha: str,
    source_dirty: bool,
) -> None:
    project = (REPO_ROOT / target["project_dir"]).resolve()
    if not project.is_dir():
        raise TestFailure(f"project does not exist: {project}")
    configured_build_types = tuple(target.get("build", {}))
    if build_type not in configured_build_types:
        raise TestFailure(
            f"{name} does not configure build type {build_type!r}; available: "
            + ", ".join(configured_build_types)
        )
    protocol = (
        target.get("hw08_qualification_protocol")
        if throughput_qualification
        else target.get("hw08_protocol", "marker") if case == "HW08" else "marker"
    )
    allowed_build_types = (
        target.get("hw08_allowed_build_types", ("debug", "release"))
        if protocol == "gated-throughput"
        else ("debug", "release")
    )
    if build_type not in allowed_build_types:
        raise TestFailure(
            f"{name} {case} protocol {protocol} only supports: "
            + ", ".join(allowed_build_types)
        )
    if protocol not in ("marker", "gated-throughput"):
        raise TestFailure(f"unsupported capture protocol: {protocol!r}")
    if case == "HW04" and target["mcu"] == "STM32F042G6" and profile != 0:
        raise TestFailure("STM32F042G6 HW04 only supports the C profile (0)")
    check_fixture(target)
    evidence = evidence_root / case / name / build_type / evidence_variant(
        case,
        profile,
        up_size,
        float_fast,
        skip_asm,
        resource_profile,
        protocol,
        throughput_qualification,
    )
    if repeated:
        evidence = evidence / f"run-{repetition}"
    if evidence.exists():
        raise TestFailure(f"refusing to overwrite evidence: {evidence}")
    run_id = secrets.randbits(32) or 1
    values = {
        "jobs": jobs,
        "cmake_build": build_type.capitalize(),
        "case_number": int(case[2:]),
        "profile": profile,
        "up_size": up_size,
        "mcu_family": target.get("mcu_family", -1),
        "irq_handler": target.get("irq_handler", "unused"),
        "library_sha": library_sha,
        "float_fast": float_fast,
        "skip_asm": skip_asm,
        "resource_profile": resource_profile,
        "gated_throughput": 1 if protocol == "gated-throughput" else 0,
        "run_id": run_id,
        "tp_base_ticks": target.get("tp_base_ticks", 0),
        "tp_base_cycles": target.get("tp_base_cycles", 0),
        "tp_remainder_step": target.get("tp_remainder_step", 0),
        "tp_remainder_denom": target.get("tp_remainder_denom", 1),
    }
    environment = os.environ.copy()
    toolchain_bin = Path(target["toolchain_bin"])
    if not toolchain_bin.is_dir():
        raise TestFailure(f"toolchain directory does not exist: {toolchain_bin}")
    environment["PATH"] = str(toolchain_bin) + os.pathsep + environment.get("PATH", "")
    build_commands = target["build"][build_type]
    if target["build_system"] == "cmake":
        cube_cmake = locate_cube_cmake()
        environment["PATH"] = (
            str(cube_cmake.parent) + os.pathsep + environment["PATH"]
        )
        values.update(
            {
                "cube_cmake": str(cube_cmake),
                "cmake_overlay": str(HERE / "support" / "hw_test_overlay.cmake"),
                "repo_root": str(REPO_ROOT),
            }
        )
        build_commands = cmake_build_commands(build_type)
    environment.update(
        {
            "HW_TEST_CASE": str(int(case[2:])),
            "HW_TEST_PROFILE": str(profile),
            "HW_TEST_UP_SIZE": str(up_size),
            "HW_TEST_MCU_FAMILY": str(target.get("mcu_family", -1)),
            "HW_TEST_IRQ_HANDLER": str(target.get("irq_handler", "unused")),
            "HW_TEST_IRQ_SOURCE": str(target.get("irq_source", "unused.c")),
            "HW_TEST_LIBRARY_SHA": library_sha,
            "HW_TEST_MAKE_OVERLAY": "1" if target["build_system"] == "make" else "0",
            "HW08_GATED_THROUGHPUT": "1" if protocol == "gated-throughput" else "0",
            "HW08_FLOAT_FAST": str(float_fast),
            "HW08_SKIP_ASM": str(skip_asm),
            "HW08_RESOURCE_PROFILE": str(resource_profile),
            "HW08_TP_RUN_ID": str(run_id),
            "HW08_TP_BASE_TICKS": str(target.get("tp_base_ticks", 0)),
            "HW08_TP_BASE_CYCLES": str(target.get("tp_base_cycles", 0)),
            "HW08_TP_REMAINDER_STEP": str(target.get("tp_remainder_step", 0)),
            "HW08_TP_REMAINDER_DENOM": str(target.get("tp_remainder_denom", 1)),
        }
    )
    if dry_run:
        print("$ env " + " ".join(f"{key}={value}" for key, value in environment.items() if key.startswith("HW")))
        run_commands(build_commands, project, evidence / "build.log", values, environment, True)
        if protocol == "gated-throughput":
            print(f"$ {target['nm']} {target['elf']}  # locate {target['result_symbol']}")
        if not build_only:
            run_commands(target["flash"][build_type], project, evidence / "flash.log", values, environment, True)
            print(
                f"$ JLinkExe -Device {target['mcu']} -if SWD "
                f"-Speed {target['jlink_speed']} -RTTTelnetPort {target['rtt_port']} ..."
            )
            print(f"$ JLinkRTTClient -RTTTelnetPort {target['rtt_port']} -LocalEcho Off")
            if protocol == "gated-throughput":
                print(f"$ JLinkExe ...  # write gate run_id=0x{run_id:08X}, capture, savebin")
        return
    evidence.mkdir(parents=True)
    metadata = {
        "target": name,
        "mcu": target["mcu"],
        "core": target["core"],
        "build_system": target["build_system"],
        "build_type": build_type,
        "case": case,
        "profile": profile,
        "up_size": up_size,
        "make_overlay": target["build_system"] == "make",
        "wrap_printf": case == "HW05",
        "float_fast": float_fast,
        "skip_asm": skip_asm,
        "resource_profile": resource_profile,
        "protocol": protocol,
        "throughput_qualification": throughput_qualification,
        "repetition": repetition,
        "hw07_start_gate": target.get("hw07_start_gate") if case == "HW07" else None,
        "hw08_gate_symbol": (
            target.get("hw08_gate_symbol")
            if case == "HW08" and not build_only
            else None
        ),
        "project_dir": str(project),
        "cmake_overlay": target["build_system"] == "cmake",
        "started_at": dt.datetime.now().astimezone().isoformat(),
        "library_sha": library_sha,
        "source_dirty": source_dirty,
    }
    if protocol == "gated-throughput":
        metadata["run_id"] = run_id
    (evidence / "metadata.json").write_text(
        json.dumps(metadata, indent=2, ensure_ascii=True) + "\n", encoding="utf-8"
    )
    run_commands(build_commands, project, evidence / "build.log", values, environment, False)
    archive_artifacts(project, target, evidence, values)
    result_address: int | None = None
    marker_gate_address: int | None = None
    entry_address: int | None = None
    entry_address = locate_symbol(
        find_elf(project, target, build_type), target, evidence, "HW_TestEntry"
    )
    if build_only:
        (evidence / "result.txt").write_text("BUILD PASS\n", encoding="ascii")
        return
    if protocol == "gated-throughput":
        result_address = locate_symbol(find_elf(project, target, build_type), target, evidence)
        (evidence / "gate.json").write_text(
            json.dumps(
                {"run_id": run_id, "result_address": result_address},
                indent=2,
                ensure_ascii=True,
            ) + "\n",
            encoding="utf-8",
        )
    elif case == "HW08" and target.get("hw08_gate_symbol"):
        marker_gate_address = locate_symbol(
            find_elf(project, target, build_type), target, evidence,
            str(target["hw08_gate_symbol"]),
        )
        (evidence / "gate.json").write_text(
            json.dumps(
                {
                    "symbol": target["hw08_gate_symbol"],
                    "address": marker_gate_address,
                    "value": int(target["hw08_gate_value"]),
                },
                indent=2,
                ensure_ascii=True,
            ) + "\n",
            encoding="utf-8",
        )
    flash_log = evidence / "flash.log"
    run_commands(target["flash"][build_type], project, flash_log, values, environment, False)
    require_no_log_failure(flash_log, "flash")
    if protocol == "marker":
        if case == "HW01":
            marker = f"HW-01|{target['mcu']}|"
            first = capture_rtt(
                target, evidence, [marker, f"|SHA={library_sha}|"], probe_serial,
                entry_address=entry_address,
            )
            first_value = heartbeat(first, target)
            (evidence / "rtt-first.raw").write_bytes(first)
            reconnect_dir = evidence / "reconnect"
            reconnect_dir.mkdir()
            second = capture_rtt(
                target,
                reconnect_dir,
                [marker, f"|SHA={library_sha}|"],
                probe_serial,
                reset_target=False,
                minimum_heartbeat=first_value,
            )
            second_value = heartbeat(second, target)
            (evidence / "rtt-reconnect.raw").write_bytes(second)
            if second_value <= first_value:
                raise TestFailure(
                    f"HW01 heartbeat did not advance across reconnect: {first_value} -> {second_value}"
                )
            data = first + second
        elif case == "HW06":
            prefix = "HW-06"
            # The fixture emits its first report before the first 10 ms LIVE tick.
            # Keep each RTT session open until it proves that business work advances.
            expected = [
                f"{prefix}|BEGIN|",
                f"{prefix}|END|",
                f"{prefix}|LIVE|SEQ=",
            ]
            first = capture_rtt(
                target, evidence, expected, probe_serial,
                entry_address=entry_address,
            )
            verify_output(first, target, build_type, case, profile, library_sha)
            first_sequence = live_sequence(first)
            (evidence / "rtt-initial.raw").write_bytes(first)
            time.sleep(int(target["disconnect_seconds"]))
            recovery_dir = evidence / "recovery"
            recovery_dir.mkdir()
            second = capture_rtt(
                target, recovery_dir, expected, probe_serial, reset_target=False
            )
            verify_output(second, target, build_type, case, profile, library_sha)
            second_sequence = live_sequence(second)
            (evidence / "rtt-recovery.raw").write_bytes(second)
            if second_sequence <= first_sequence:
                raise TestFailure(
                    "HW06 live sequence did not advance while the client was disconnected: "
                    f"{first_sequence} -> {second_sequence}"
                )
            data = first + second
        else:
            prefix = case[:2] + "-" + case[2:]
            expected = [f"{prefix}|BEGIN|", f"{prefix}|END|"]
            data = capture_rtt(
                target,
                evidence,
                expected,
                probe_serial,
                rtt_channel=1 if case == "HW03" and profile == 6 else 0,
                start_gate=(
                    ("HW-07|READY|WAIT_HOST|", target["hw07_start_gate"].encode("ascii"))
                    if case == "HW07" and target.get("hw07_start_gate")
                    else None
                ),
                memory_start_gate=(
                    (
                        "HW-08|THROUGHPUT|READY|WAIT_HOST|",
                        marker_gate_address,
                        int(target["hw08_gate_value"]),
                    )
                    if case == "HW08" and marker_gate_address is not None
                    else None
                ),
                entry_address=entry_address,
            )
        (evidence / "rtt.raw").write_bytes(data)
        if case not in ("HW01", "HW06"):
            verify_output(data, target, build_type, case, profile, library_sha)
        if case == "HW08":
            analysis = verify_hw08_marker_throughput(data, target)
            if analysis:
                (evidence / "throughput-analysis.txt").write_text(
                    analysis, encoding="utf-8"
                )
    else:
        assert result_address is not None
        capture, result_data = capture_gated_throughput(
            target, evidence, result_address, entry_address, run_id, probe_serial
        )
        analysis = analyze_gated_throughput(result_data, capture, target, run_id)
        (evidence / "analysis.txt").write_text(analysis, encoding="utf-8")
        if "OVERALL\tPASS\n" not in analysis:
            raise TestFailure("gated throughput verdict failed; see analysis.txt")
    (evidence / "result.txt").write_text("PASS\n", encoding="ascii")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument("--target", action="append", help="target name; repeatable")
    selection.add_argument("--mcu", help="run both Make and CMake projects for one MCU")
    selection.add_argument("--all", action="store_true", help="run every configured target")
    parser.add_argument("--list", action="store_true", help="list configured targets")
    parser.add_argument("--build-type", choices=("debug", "release"), default="release")
    parser.add_argument("--case", choices=CASES, default="HW08", help="HW01 through HW08")
    parser.add_argument("--profile", type=int, default=0, help="case-specific numeric profile")
    parser.add_argument("--up-size", type=int, default=256, help="HW06 test up-buffer size")
    parser.add_argument("--float-fast", type=int, choices=(0, 1), default=0, help="HW08 legacy float path")
    parser.add_argument("--skip-asm", type=int, choices=(0, 1), default=0, help="HW08 Skip implementation")
    parser.add_argument("--resource-profile", type=int, choices=range(0, 7), default=0, help="HW08 resource-only profile")
    parser.add_argument("--build-only", action="store_true", help="stop after build and archive")
    parser.add_argument("--dry-run", action="store_true", help="validate and print commands only")
    parser.add_argument("--yes", action="store_true", help="do not prompt when changing MCU")
    parser.add_argument("--jobs", type=int, default=max(1, os.cpu_count() or 1))
    parser.add_argument("--repeat", type=int, default=1, help="repeat each selected target")
    parser.add_argument(
        "--throughput-qualification",
        action="store_true",
        help="run the target's formal HW08 gated throughput qualification",
    )
    parser.add_argument("--probe-serial", help="J-Link serial number")
    parser.add_argument("--config", type=Path, default=DEFAULT_CONFIG)
    parser.add_argument("--evidence-dir", type=Path)
    project_selection = parser.add_mutually_exclusive_group()
    project_selection.add_argument(
        "--local",
        action="store_true",
        help="use the current working directory as the single target project",
    )
    project_selection.add_argument(
        "--project-dir",
        type=Path,
        help="override the single target project directory; relative to the current directory",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        defaults, configured = load_config(args.config.resolve())
        targets = {name: merged_target(defaults, item) for name, item in configured.items()}
        if args.list:
            for name, target in targets.items():
                qualification = (
                    ", qualification: gated-throughput"
                    if target.get("hw08_qualification_protocol") == "gated-throughput"
                    else ""
                )
                print(
                    f"{name:12} {target['mcu']:14} {target['build_system']:5} "
                    f"HW01-HW08 (HW08: {target.get('hw08_protocol', 'marker')}"
                    f"{qualification})"
                )
            return 0
        if args.target:
            unknown = sorted(set(args.target) - set(targets))
            if unknown:
                raise TestFailure("unknown targets: " + ", ".join(unknown))
            selected = args.target
        elif args.mcu:
            selected = [name for name, target in targets.items() if target["mcu"] == args.mcu]
            if not selected:
                raise TestFailure(f"no targets configured for MCU {args.mcu!r}")
        elif args.all:
            selected = list(targets)
        else:
            raise TestFailure("select --target, --mcu or --all (or use --list)")
        project_override: Path | None = None
        if args.local or args.project_dir is not None:
            if not args.target or len(args.target) != 1:
                raise TestFailure(
                    "--local/--project-dir requires exactly one --target"
                )
            project_override = (
                Path.cwd()
                if args.local
                else args.project_dir.expanduser().resolve()
            )
            targets[selected[0]] = {
                **targets[selected[0]],
                "project_dir": str(project_override),
            }
        if args.repeat < 1:
            raise TestFailure("--repeat must be at least 1")
        if args.profile < 0:
            raise TestFailure("--profile must be non-negative")
        if args.up_size < 1:
            raise TestFailure("--up-size must be at least 1")
        profile_limits = {"HW03": 6, "HW04": 1, "HW05": 3}
        if args.case in profile_limits and args.profile > profile_limits[args.case]:
            raise TestFailure(
                f"{args.case} profile must be in 0..{profile_limits[args.case]}"
            )
        if args.case not in profile_limits and args.profile != 0:
            raise TestFailure(f"{args.case} does not define numeric profiles")
        if args.case != "HW08" and (
            args.float_fast or args.skip_asm or args.resource_profile
            or args.throughput_qualification
        ):
            raise TestFailure("HW08-specific options require --case HW08")
        if args.resource_profile and not args.build_only:
            raise TestFailure("HW08 resource profiles are build-only measurements")
        args.float_fast, args.skip_asm = resolve_hw08_options(
            args.resource_profile, args.float_fast, args.skip_asm
        )
        if args.throughput_qualification:
            if args.float_fast or args.skip_asm or args.resource_profile:
                raise TestFailure(
                    "--throughput-qualification does not support marker/resource options"
                )
            for name in selected:
                target = targets[name]
                if target.get("hw08_qualification_protocol") != "gated-throughput":
                    raise TestFailure(
                        f"{name} does not configure formal HW08 throughput qualification"
                    )
                minimum = int(target.get("hw08_qualification_min_repetitions", 1))
                if args.repeat < minimum and not args.build_only and not args.dry_run:
                    raise TestFailure(
                        f"{name} formal HW08 qualification requires --repeat {minimum} or greater"
                    )

        library_sha, source_dirty = source_identity()
        stamp = dt.datetime.now().astimezone().strftime("%Y%m%d_%H%M%S")
        evidence_base = project_override or REPO_ROOT
        evidence_root = (
            args.evidence_dir
            or evidence_base / "TEST_EVIDENCE" / f"HW_RUN_{stamp}"
        ).expanduser().resolve()
        previous_mcu: str | None = None
        for name in selected:
            target = targets[name]
            mcu = target["mcu"]
            if (
                args.case == "HW08"
                and target.get("hw08_protocol") == "gated-throughput"
                and (args.float_fast or args.skip_asm or args.resource_profile)
            ):
                raise TestFailure(
                    f"{name} gated-throughput does not support marker/resource options"
                )
            if (
                previous_mcu != mcu
                and not args.yes
                and not args.dry_run
                and not args.build_only
            ):
                input(f"Connect {mcu} and press Enter to run {name}: ")
            for repetition in range(1, args.repeat + 1):
                suffix = f", run {repetition}/{args.repeat}" if args.repeat > 1 else ""
                print(f"==> {name} ({args.build_type}{suffix})")
                run_target(
                    name,
                    target,
                    args.build_type,
                    evidence_root,
                    args.jobs,
                    args.probe_serial,
                    args.dry_run,
                    args.build_only,
                    repetition,
                    args.repeat > 1,
                    args.case,
                    args.profile,
                    args.up_size,
                    args.float_fast,
                    args.skip_asm,
                    args.resource_profile,
                    args.throughput_qualification,
                    library_sha,
                    source_dirty,
                )
            previous_mcu = mcu
        print(f"evidence: {evidence_root}")
        return 0
    except (OSError, ValueError, TestFailure) as error:
        print(f"HW test failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
