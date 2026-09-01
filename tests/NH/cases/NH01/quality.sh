#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/../../../.." && pwd)
OUTPUT_DIR=$1
HOST_CC=${HOST_CC:-cc}
CLANG=${NH01_CLANG:-clang}
CPPCHECK=${NH01_CPPCHECK:-cppcheck}
TOOLCHAIN_PREFIX=${TOOLCHAIN_PREFIX:-arm-none-eabi-}
ARM_CC=${TOOLCHAIN_PREFIX}gcc
ARM_NM=${TOOLCHAIN_PREFIX}nm

mkdir -p "$OUTPUT_DIR/host" "$OUTPUT_DIR/coverage/raw" "$OUTPUT_DIR/arm"

require_command() {
  command -v "$1" >/dev/null 2>&1 || {
    printf 'NH01 missing required tool: %s\n' "$1" >&2
    exit 1
  }
}

llvm_tool() {
  name=$1
  if command -v "$name" >/dev/null 2>&1; then
    command -v "$name"
  elif command -v xcrun >/dev/null 2>&1; then
    xcrun -f "$name"
  else
    printf 'NH01 missing required LLVM tool: %s\n' "$name" >&2
    exit 1
  fi
}

require_command "$HOST_CC"
require_command "$CLANG"
require_command "$CPPCHECK"
require_command "$ARM_CC"
require_command "$ARM_NM"
LLVM_PROFDATA=$(llvm_tool llvm-profdata)
LLVM_COV=$(llvm_tool llvm-cov)

host_sources="
$PROJECT_DIR/rtt_printf.c
$PROJECT_DIR/rtt_log.c
$PROJECT_DIR/rtt_float.c
$SCRIPT_DIR/rtt_log_test.c
"

build_and_run_sanitizer() {
  name=$1
  shift
  executable="$OUTPUT_DIR/host/$name"
  # Word splitting is intentional for the fixed source list.
  # shellcheck disable=SC2086
  "$HOST_CC" -std=c11 -Wall -Wextra -Werror -pedantic -g \
    -fno-omit-frame-pointer "$@" -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
    $host_sources -o "$executable"
  "$executable" >"$OUTPUT_DIR/host/$name.out" 2>"$OUTPUT_DIR/host/$name.err"
  test ! -s "$OUTPUT_DIR/host/$name.err"
}

build_and_run_sanitizer asan -fsanitize=address
build_and_run_sanitizer ubsan -fsanitize=undefined -fno-sanitize-recover=undefined

"$CPPCHECK" --enable=warning,performance,portability --error-exitcode=1 \
  --std=c11 --suppress=missingIncludeSystem \
  --suppress=normalCheckLevelMaxBranches --suppress=toomanyconfigs \
  -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
  "$PROJECT_DIR/rtt_printf.c" "$PROJECT_DIR/rtt_log.c" \
  "$PROJECT_DIR/rtt_float.c" "$PROJECT_DIR/rtt_log.h" \
  "$PROJECT_DIR/rtt_float.h" \
  >"$OUTPUT_DIR/host/cppcheck.out" 2>"$OUTPUT_DIR/host/cppcheck.err"

coverage_executable="$OUTPUT_DIR/coverage/nh01"
"$CLANG" -std=c11 -Wall -Wextra -Werror -pedantic -O0 -g \
  -fprofile-instr-generate -fcoverage-mapping \
  -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
  "$PROJECT_DIR/rtt_printf.c" "$PROJECT_DIR/rtt_log.c" \
  "$PROJECT_DIR/rtt_float.c" "$SCRIPT_DIR/rtt_log_test.c" \
  -o "$coverage_executable"
LLVM_PROFILE_FILE="$OUTPUT_DIR/coverage/raw/nh01.profraw" \
  "$coverage_executable" >"$OUTPUT_DIR/coverage/run.out"
"$LLVM_PROFDATA" merge -sparse "$OUTPUT_DIR/coverage/raw/nh01.profraw" \
  -o "$OUTPUT_DIR/coverage/nh01.profdata"
"$LLVM_COV" report "$coverage_executable" \
  -instr-profile="$OUTPUT_DIR/coverage/nh01.profdata" \
  "$PROJECT_DIR/rtt_printf.c" "$PROJECT_DIR/rtt_log.c" \
  "$PROJECT_DIR/rtt_float.c" >"$OUTPUT_DIR/coverage/report.txt"
"$LLVM_COV" export "$coverage_executable" \
  -instr-profile="$OUTPUT_DIR/coverage/nh01.profdata" \
  "$PROJECT_DIR/rtt_printf.c" "$PROJECT_DIR/rtt_log.c" \
  "$PROJECT_DIR/rtt_float.c" >"$OUTPUT_DIR/coverage/export.json"

awk '
  /^TOTAL/ {
    line = $10; branch = $13
    sub(/%$/, "", line); sub(/%$/, "", branch)
    printf "line_coverage=%s%%\nbranch_coverage=%s%%\n", line, branch
    if ((line + 0) < 90 || (branch + 0) < 80) exit 1
    found = 1
  }
  END { if (!found) exit 1 }
' "$OUTPUT_DIR/coverage/report.txt" >"$OUTPUT_DIR/coverage/thresholds.txt"

build_arm() {
  name=$1
  optimization=$2
  shift 2
  build_dir="$OUTPUT_DIR/arm/${name}_${optimization}"
  mkdir -p "$build_dir"
  common="-std=c11 -fdata-sections -ffunction-sections -Wall -Wextra -Werror"

  # Word splitting is intentional for fixed compiler flags.
  # shellcheck disable=SC2086
  "$ARM_CC" "$@" "-$optimization" $common -DLOG_ENABLE_TYPED=1 \
    -DLOG_ENABLE_TYPED_FLOAT=1 -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
    -c "$PROJECT_DIR/RTT/SEGGER_RTT.c" -o "$build_dir/SEGGER_RTT.o"
  for source in rtt_printf.c rtt_log.c rtt_float.c; do
    # shellcheck disable=SC2086
    "$ARM_CC" "$@" "-$optimization" $common -DLOG_ENABLE_TYPED=1 \
      -DLOG_ENABLE_TYPED_FLOAT=1 -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
      -c "$PROJECT_DIR/$source" -o "$build_dir/${source%.c}.o"
  done
  "$ARM_CC" "$@" "-$optimization" -x assembler-with-cpp \
    -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
    -c "$PROJECT_DIR/RTT/SEGGER_RTT_ASM_ARMv7M.S" \
    -o "$build_dir/SEGGER_RTT_ASM_ARMv7M.o"
  # shellcheck disable=SC2086
  "$ARM_CC" "$@" "-$optimization" $common -DLOG_ENABLE_TYPED=1 \
    -DLOG_ENABLE_TYPED_FLOAT=1 -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
    -c "$SCRIPT_DIR/arm_probe.c" -o "$build_dir/arm_probe.o"
  "$ARM_CC" "$@" "-$optimization" --specs=nosys.specs -nostartfiles \
    -Wl,-e,main -Wl,--gc-sections \
    "$build_dir/arm_probe.o" "$build_dir/SEGGER_RTT.o" \
    "$build_dir/rtt_printf.o" "$build_dir/rtt_log.o" \
    "$build_dir/rtt_float.o" "$build_dir/SEGGER_RTT_ASM_ARMv7M.o" \
    -lm -lc -lnosys -o "$build_dir/probe.elf"
  "$ARM_NM" -u "$build_dir/probe.elf" >"$build_dir/undefined.txt"
}

for optimization in O0 Os; do
  build_arm m0 "$optimization" -mcpu=cortex-m0 -mthumb
  build_arm m3 "$optimization" -mcpu=cortex-m3 -mthumb
  build_arm m4f "$optimization" -mcpu=cortex-m4 -mthumb \
    -mfpu=fpv4-sp-d16 -mfloat-abi=hard
  build_arm m7 "$optimization" -mcpu=cortex-m7 -mthumb \
    -mfpu=fpv5-d16 -mfloat-abi=hard
done

if grep -E '__aeabi_(uidiv|idiv|uldiv|ldiv)' "$OUTPUT_DIR/arm/m0_"*/undefined.txt; then
  printf 'NH01 M0 unexpectedly depends on an integer division helper\n' >&2
  exit 1
fi

printf '%s\n' \
  'NH01 QUALITY PASS: ASan, UBSan, Cppcheck, coverage, and 8 Arm ELF builds'
