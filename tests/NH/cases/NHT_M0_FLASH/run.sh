#!/bin/sh
set -eu

TOOLCHAIN_PREFIX=${TOOLCHAIN_PREFIX:-arm-none-eabi-}
ARM_CC=${TOOLCHAIN_PREFIX}gcc
ARM_SIZE=${TOOLCHAIN_PREFIX}size
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/../../../.." && pwd)
if [ -n "${NH_OUTPUT_DIR:-}" ]; then
  BUILD_ROOT=$NH_OUTPUT_DIR
  mkdir -p "$BUILD_ROOT"
  KEEP_BUILD=1
else
  BUILD_ROOT=$(mktemp -d "${TMPDIR:-/tmp}/rtt-nht-m0-flash.XXXXXX")
  KEEP_BUILD=0
fi
M0_FLAGS="-mcpu=cortex-m0 -mthumb"

cleanup() {
  if [ "$KEEP_BUILD" -eq 0 ]; then
    rm -rf "$BUILD_ROOT"
  fi
}
trap cleanup EXIT INT TERM

build_config() {
  name=$1
  fast_path=$2
  build="$BUILD_ROOT/$name"
  mkdir -p "$build"

  # shellcheck disable=SC2086
  "$ARM_CC" $M0_FLAGS -Os -std=c11 -ffunction-sections -fdata-sections \
    -Wall -Wextra -Werror -DRTT_LOG_FLOAT_FAST_PATH="$fast_path" \
    -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
    -c "$PROJECT_DIR/RTT/SEGGER_RTT.c" -o "$build/SEGGER_RTT.o"
  for source in rtt_printf.c rtt_log.c rtt_float.c; do
    # shellcheck disable=SC2086
    "$ARM_CC" $M0_FLAGS -Os -std=c11 -ffunction-sections -fdata-sections \
      -Wall -Wextra -Werror -DRTT_LOG_FLOAT_FAST_PATH="$fast_path" \
      -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
      -c "$PROJECT_DIR/$source" -o "$build/${source%.c}.o"
  done
  # shellcheck disable=SC2086
  "$ARM_CC" $M0_FLAGS -Os -x assembler-with-cpp \
    -DRTT_LOG_FLOAT_FAST_PATH="$fast_path" \
    -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
    -c "$PROJECT_DIR/RTT/SEGGER_RTT_ASM_ARMv7M.S" \
    -o "$build/SEGGER_RTT_ASM_ARMv7M.o"
  # shellcheck disable=SC2086
  "$ARM_CC" $M0_FLAGS -Os -std=c11 -ffunction-sections -fdata-sections \
    -Wall -Wextra -Werror -DRTT_LOG_FLOAT_FAST_PATH="$fast_path" \
    -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
    -c "$SCRIPT_DIR/rtt_float_flash_probe.c" -o "$build/probe.o"

  # shellcheck disable=SC2086
  "$ARM_CC" $M0_FLAGS -Os --specs=nosys.specs -nostartfiles \
    -Wl,-e,main -Wl,--gc-sections -Wl,-Map="$build/result.map" \
    "$build/probe.o" "$build/SEGGER_RTT.o" \
    "$build/rtt_printf.o" "$build/rtt_log.o" "$build/rtt_float.o" \
    "$build/SEGGER_RTT_ASM_ARMv7M.o" \
    -lm -lc -lnosys -o "$build/result.elf"
  "$ARM_SIZE" "$build/result.elf" >"$build/size.txt"
}

flash_size() {
  awk 'NR == 2 { print $1 + $2 }' "$BUILD_ROOT/$1/size.txt"
}

build_config fast_on 1
build_config fast_off 0
fast_on_flash=$(flash_size fast_on)
fast_off_flash=$(flash_size fast_off)
{
  printf 'fast_on_flash=%s\n' "$fast_on_flash"
  printf 'fast_off_flash=%s\n' "$fast_off_flash"
  printf 'saved=%s\n' "$((fast_on_flash - fast_off_flash))"
} >"$BUILD_ROOT/sizes.txt"

if test "$fast_off_flash" -ge "$fast_on_flash"; then
  printf 'NHT_M0_FLASH disabling float fast-path did not reduce Flash: on=%s off=%s\n' \
    "$fast_on_flash" "$fast_off_flash" >&2
  exit 1
fi
printf 'NHT_M0_FLASH PASS: M0 fast-on=%s B fast-off=%s B saved=%s B\n' \
  "$fast_on_flash" "$fast_off_flash" "$((fast_on_flash - fast_off_flash))"
