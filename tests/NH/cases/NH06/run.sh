#!/bin/sh
set -eu

HOST_CC=${HOST_CC:-cc}
TOOLCHAIN_PREFIX=${TOOLCHAIN_PREFIX:-arm-none-eabi-}
ARM_CC=${TOOLCHAIN_PREFIX}gcc
ARM_NM=${TOOLCHAIN_PREFIX}nm
ARM_OBJDUMP=${TOOLCHAIN_PREFIX}objdump
ARM_SIZE=${TOOLCHAIN_PREFIX}size
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/../../../.." && pwd)
if [ -n "${NH_OUTPUT_DIR:-}" ]; then
  BUILD_ROOT=$NH_OUTPUT_DIR
  mkdir -p "$BUILD_ROOT"
  KEEP_BUILD=1
else
  BUILD_ROOT=$(mktemp -d "${TMPDIR:-/tmp}/rtt-nh06-test.XXXXXX")
  KEEP_BUILD=0
fi
M0_FLAGS="-mcpu=cortex-m0 -mthumb"
M7_FLAGS="-mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard"

cleanup() {
  if [ "$KEEP_BUILD" -eq 0 ]; then
    rm -rf "$BUILD_ROOT"
  fi
}
trap cleanup EXIT INT TERM

build_host() {
  name=$1
  shift
  "$HOST_CC" -std=c11 -Wall -Wextra -Werror -pedantic \
    "$@" \
    -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
    "$PROJECT_DIR/rtt_printf.c" \
    "$PROJECT_DIR/rtt_log.c" \
    "$PROJECT_DIR/rtt_float.c" \
    "$SCRIPT_DIR/../NH01/rtt_log_test.c" \
    -o "$BUILD_ROOT/host_$name"
  "$BUILD_ROOT/host_$name"
}

build_arm_config() {
  name=$1
  shift
  build="$BUILD_ROOT/arm_$name"
  mkdir -p "$build"

  for source in SEGGER_RTT.c; do
    # shellcheck disable=SC2086
    "$ARM_CC" $M0_FLAGS -Os -std=c11 -ffunction-sections -fdata-sections \
      -Wall -Wextra -Werror "$@" \
      -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
      -c "$PROJECT_DIR/RTT/$source" -o "$build/${source%.c}.o"
  done
  for source in rtt_printf.c rtt_log.c rtt_float.c; do
    # shellcheck disable=SC2086
    "$ARM_CC" $M0_FLAGS -Os -std=c11 -ffunction-sections -fdata-sections \
      -Wall -Wextra -Werror "$@" \
      -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
      -c "$PROJECT_DIR/$source" -o "$build/${source%.c}.o"
  done
  # shellcheck disable=SC2086
  "$ARM_CC" $M0_FLAGS -Os -x assembler-with-cpp "$@" \
    -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
    -c "$PROJECT_DIR/RTT/SEGGER_RTT_ASM_ARMv7M.S" \
    -o "$build/SEGGER_RTT_ASM_ARMv7M.o"
  # shellcheck disable=SC2086
  "$ARM_CC" $M0_FLAGS -Os -std=c11 -ffunction-sections -fdata-sections \
    -Wall -Wextra -Werror "$@" \
    -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
    -c "$SCRIPT_DIR/rtt_config_nh06_probe.c" -o "$build/probe.o"

  # shellcheck disable=SC2086
  "$ARM_CC" $M0_FLAGS -Os --specs=nosys.specs -nostartfiles \
    -Wl,-e,main -Wl,--gc-sections -Wl,-Map="$build/result.map" \
    "$build/probe.o" "$build/SEGGER_RTT.o" \
    "$build/rtt_printf.o" "$build/rtt_log.o" "$build/rtt_float.o" \
    "$build/SEGGER_RTT_ASM_ARMv7M.o" \
    -lm -lc -lnosys -o "$build/result.elf"
  "$ARM_NM" "$build/result.elf" >"$build/symbols.txt"
  "$ARM_OBJDUMP" -h "$build/result.elf" >"$build/sections.txt"
  printf 'NH06 ARM config PASS: %s map=yes symbols=yes objdump=yes\n' "$name"
}

require_symbol() {
  config=$1
  symbol=$2
  if ! grep -Eq "[[:space:]]$symbol$" \
      "$BUILD_ROOT/arm_$config/symbols.txt"; then
    printf 'NH06 %s missing symbol %s\n' "$config" "$symbol" >&2
    return 1
  fi
}

reject_symbol() {
  config=$1
  symbol=$2
  if grep -Eq "[[:space:]]$symbol$" \
      "$BUILD_ROOT/arm_$config/symbols.txt"; then
    printf 'NH06 %s retained disabled symbol %s\n' "$config" "$symbol" >&2
    return 1
  fi
}

require_object_reference() {
  config=$1
  object=$2
  symbol=$3
  if ! "$ARM_NM" -u "$BUILD_ROOT/arm_$config/$object" | \
      grep -Eq "[[:space:]]$symbol$"; then
    printf 'NH06 %s %s missing reference to %s\n' \
      "$config" "$object" "$symbol" >&2
    return 1
  fi
}

reject_object_reference() {
  config=$1
  object=$2
  symbol=$3
  if "$ARM_NM" -u "$BUILD_ROOT/arm_$config/$object" | \
      grep -Eq "[[:space:]]$symbol$"; then
    printf 'NH06 %s %s retained reference to %s\n' \
      "$config" "$object" "$symbol" >&2
    return 1
  fi
}

flash_size() {
  "$ARM_SIZE" "$BUILD_ROOT/arm_$1/result.elf" | \
    awk 'NR == 2 { print $1 + $2 }'
}

build_armv7_path() {
  name=$1
  define=$2
  build="$BUILD_ROOT/armv7_$name"
  mkdir -p "$build"

  # shellcheck disable=SC2086
  "$ARM_CC" $M7_FLAGS -Os -std=c11 -ffunction-sections -fdata-sections \
    -Wall -Wextra -Werror $define \
    -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
    -c "$PROJECT_DIR/RTT/SEGGER_RTT.c" -o "$build/SEGGER_RTT.o"
  # shellcheck disable=SC2086
  "$ARM_CC" $M7_FLAGS -Os -x assembler-with-cpp $define \
    -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
    -c "$PROJECT_DIR/RTT/SEGGER_RTT_ASM_ARMv7M.S" \
    -o "$build/SEGGER_RTT_ASM_ARMv7M.o"
  # shellcheck disable=SC2086
  "$ARM_CC" $M7_FLAGS -Os -std=c11 -ffunction-sections -fdata-sections \
    -Wall -Wextra -Werror $define \
    -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
    -c "$SCRIPT_DIR/rtt_config_nh06_probe.c" -o "$build/probe.o"

  "$ARM_NM" "$build/SEGGER_RTT.o" >"$build/c-symbols.txt"
  "$ARM_NM" "$build/SEGGER_RTT_ASM_ARMv7M.o" >"$build/asm-symbols.txt"
  "$ARM_OBJDUMP" -d "$build/SEGGER_RTT.o" >"$build/c-disassembly.txt"
  "$ARM_OBJDUMP" -d "$build/SEGGER_RTT_ASM_ARMv7M.o" \
    >"$build/asm-disassembly.txt"
  # shellcheck disable=SC2086
  "$ARM_CC" $M7_FLAGS -dM -E $define \
    -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
    -include SEGGER_RTT.h -x c /dev/null >"$build/c-macros.txt"
  # shellcheck disable=SC2086
  "$ARM_CC" $M7_FLAGS -E -x assembler-with-cpp $define \
    -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
    "$PROJECT_DIR/RTT/SEGGER_RTT_ASM_ARMv7M.S" \
    >"$build/asm-preprocessed.txt"
}

printf '%s\n' 'NH06 host matrix start'
sh "$SCRIPT_DIR/../NH01/run.sh"
build_host float_fast_on -DRTT_LOG_FLOAT_FAST_PATH=1
build_host float_fast_off -DRTT_LOG_FLOAT_FAST_PATH=0
printf '%s\n' 'NH06 host float fast-path PASS: enabled/disabled behavior'
build_host minimal \
  -DLOG_ENABLE_INFO=0 -DLOG_ENABLE_DEBUG=0 -DLOG_ENABLE_WARN=0 \
  -DLOG_ENABLE_ERROR=0 -DLOG_ENABLE_FLOAT=0 \
  -DLOG_ENABLE_PRINT=1 -DLOG_ENABLE_STRING=1

"$HOST_CC" -std=c11 -Wall -Wextra -Werror -pedantic \
  -DRTT_LOG_ENABLE=0 \
  -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
  "$SCRIPT_DIR/rtt_disabled_side_effect_test.c" \
  -o "$BUILD_ROOT/host_side_effects"
"$BUILD_ROOT/host_side_effects"
printf '%s\n' 'NH06 host side-effects PASS: APIs=8 count=0'

build_arm_config full
build_arm_config no_color -DRTT_LOG_USE_COLOR=0
build_arm_config lite -DLOG_ENABLE_LITE=1
build_arm_config minimal \
  -DLOG_ENABLE_INFO=0 -DLOG_ENABLE_DEBUG=0 -DLOG_ENABLE_WARN=0 \
  -DLOG_ENABLE_ERROR=0 -DLOG_ENABLE_FLOAT=0 \
  -DLOG_ENABLE_PRINT=1 -DLOG_ENABLE_STRING=1
build_arm_config all_disabled -DRTT_LOG_ENABLE=0
build_arm_config info_disabled -DLOG_ENABLE_INFO=0
build_arm_config debug_disabled -DLOG_ENABLE_DEBUG=0
build_arm_config warn_disabled -DLOG_ENABLE_WARN=0
build_arm_config error_disabled -DLOG_ENABLE_ERROR=0
build_arm_config print_disabled -DLOG_ENABLE_PRINT=0
build_arm_config string_disabled -DLOG_ENABLE_STRING=0
build_arm_config float_disabled -DLOG_ENABLE_FLOAT=0
build_arm_config float_fast_on -DRTT_LOG_FLOAT_FAST_PATH=1
build_arm_config float_fast_off -DRTT_LOG_FLOAT_FAST_PATH=0

require_symbol full RTT_LogPrintf
require_symbol full RTT_LogFloat3
require_symbol full RTT_LogString
require_symbol full SEGGER_RTT_printf
reject_symbol all_disabled RTT_LogPrintf
reject_symbol all_disabled RTT_LogFloat3
reject_symbol all_disabled RTT_LogString
reject_symbol all_disabled SEGGER_RTT_printf
require_symbol minimal SEGGER_RTT_printf
require_symbol minimal RTT_LogString
reject_symbol minimal RTT_LogPrintf
reject_symbol minimal RTT_LogFloat3
reject_symbol string_disabled RTT_LogString
reject_symbol float_disabled RTT_LogFloat3
printf '%s\n' 'NH06 ARM symbol pruning PASS'

require_symbol float_fast_on RTT_LogFloat3
require_symbol float_fast_off RTT_LogFloat3
require_object_reference float_fast_on rtt_float.o SEGGER_RTT_Write
require_object_reference float_fast_on rtt_float.o SEGGER_RTT_printf
reject_object_reference float_fast_off rtt_float.o SEGGER_RTT_Write
require_object_reference float_fast_off rtt_float.o SEGGER_RTT_printf
if grep -Eq '[[:space:]]__aeabi_.*div.*$' \
    "$BUILD_ROOT/arm_float_fast_on/symbols.txt"; then
  printf '%s\n' 'NH06 M0 enabled float fast-path retained a division helper' >&2
  exit 1
fi
if grep -Eq '[[:space:]]__aeabi_.*div.*$' \
    "$BUILD_ROOT/arm_float_fast_off/symbols.txt"; then
  printf '%s\n' 'NH06 M0 disabled float fast-path retained a division helper' >&2
  exit 1
fi
fast_on_flash=$(flash_size float_fast_on)
fast_off_flash=$(flash_size float_fast_off)
if test "$fast_off_flash" -ge "$fast_on_flash"; then
  printf 'NH06 disabling float fast-path did not reduce Flash: on=%s off=%s\n' \
    "$fast_on_flash" "$fast_off_flash" >&2
  exit 1
fi
printf 'NH06 ARM float fast-path pruning PASS: on=%s off=%s saved=%s; division_helpers=none\n' \
  "$fast_on_flash" "$fast_off_flash" "$((fast_on_flash - fast_off_flash))"

build_armv7_path auto ""
if ! grep -Eq '^#define RTT_USE_ASM[[:space:]]+\(?1\)?$' \
    "$BUILD_ROOT/armv7_auto/c-macros.txt"; then
  printf '%s\n' 'NH06 auto C preprocessing did not select RTT_USE_ASM=1' >&2
  exit 1
fi
if ! grep -q 'SEGGER_RTT_ASM_WriteSkipNoLock' \
    "$BUILD_ROOT/armv7_auto/asm-preprocessed.txt"; then
  printf '%s\n' 'NH06 auto ASM preprocessing omitted the ASM implementation' >&2
  exit 1
fi
if ! grep -Eq '[[:space:]]SEGGER_RTT_ASM_WriteSkipNoLock$' \
    "$BUILD_ROOT/armv7_auto/asm-symbols.txt"; then
  printf '%s\n' 'NH06 auto ASM object is missing the ASM implementation' >&2
  exit 1
fi
if grep -Eq '[[:space:]]SEGGER_RTT_WriteSkipNoLock$' \
    "$BUILD_ROOT/armv7_auto/c-symbols.txt"; then
  printf '%s\n' 'NH06 auto C object unexpectedly retained the C implementation' >&2
  exit 1
fi
build_armv7_path forced_c "-DRTT_USE_ASM=0"
if ! grep -Eq '^#define RTT_USE_ASM[[:space:]]+0$' \
    "$BUILD_ROOT/armv7_forced_c/c-macros.txt"; then
  printf '%s\n' 'NH06 forced C preprocessing did not retain RTT_USE_ASM=0' >&2
  exit 1
fi
if grep -q 'SEGGER_RTT_ASM_WriteSkipNoLock' \
    "$BUILD_ROOT/armv7_forced_c/asm-preprocessed.txt"; then
  printf '%s\n' 'NH06 forced ASM preprocessing retained the ASM implementation' >&2
  exit 1
fi
if grep -Eq '[[:space:]]SEGGER_RTT_ASM_WriteSkipNoLock$' \
    "$BUILD_ROOT/armv7_forced_c/asm-symbols.txt"; then
  printf '%s\n' 'NH06 forced-C ASM object retained the ASM implementation' >&2
  exit 1
fi
if ! grep -Eq '[[:space:]]SEGGER_RTT_WriteSkipNoLock$' \
    "$BUILD_ROOT/armv7_forced_c/c-symbols.txt"; then
  printf '%s\n' 'NH06 forced-C object is missing the C implementation' >&2
  exit 1
fi
printf '%s\n' 'NH06 ARMv7-M path PASS: auto=ASM forced=C; define reached C+S'

conflict_log="$BUILD_ROOT/conflict.log"
if "$ARM_CC" -std=c11 $M7_FLAGS -fsyntax-only \
    -DRTT_USE_ASM=1 -DSEGGER_RTT_CPU_CACHE_LINE_SIZE=32 \
    -DSEGGER_RTT_UNCACHED_OFF=0 \
    -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
    "$PROJECT_DIR/RTT/SEGGER_RTT.c" >"$conflict_log" 2>&1; then
  printf '%s\n' 'NH06 contradictory ASM/cache configuration compiled unexpectedly' >&2
  exit 1
fi
if ! grep -Fq 'RTT_USE_ASM is not available' "$conflict_log"; then
  printf '%s\n' 'NH06 contradictory configuration failed for an unexpected reason' >&2
  sed -n '1,80p' "$conflict_log" >&2
  exit 1
fi
printf '%s\n' 'NH06 invalid configuration PASS: ASM plus cache rejected'
