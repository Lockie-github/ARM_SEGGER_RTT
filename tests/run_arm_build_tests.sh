#!/bin/sh
set -eu

TOOLCHAIN_PREFIX=${TOOLCHAIN_PREFIX:-/opt/arm-gnu-toolchain-12.2.rel1-darwin-arm64-arm-none-eabi/bin/arm-none-eabi-}
CC=${TOOLCHAIN_PREFIX}gcc
AR=${TOOLCHAIN_PREFIX}ar
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
BUILD_ROOT=$(mktemp -d "${TMPDIR:-/tmp}/rtt-log-arm.XXXXXX")

cleanup() {
  rm -rf "$BUILD_ROOT"
}
trap cleanup EXIT INT TERM

build_one() {
  name=$1
  optimization=$2
  shift 2
  build_dir="$BUILD_ROOT/${name}_${optimization}"
  mkdir -p "$build_dir"

  for source in SEGGER_RTT.c; do
    "$CC" "$@" -"$optimization" -std=c11 -ffunction-sections \
      -fdata-sections -Wall -Wextra -Werror \
      -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
      -c "$PROJECT_DIR/RTT/$source" \
      -o "$build_dir/${source%.c}.o"
  done

  for source in rtt_printf.c rtt_log.c rtt_core.c; do
    "$CC" "$@" -"$optimization" -std=c11 -ffunction-sections \
      -fdata-sections -Wall -Wextra -Werror \
      -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
      -c "$PROJECT_DIR/$source" \
      -o "$build_dir/${source%.c}.o"
  done

  "$CC" "$@" -"$optimization" -x assembler-with-cpp \
    -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
    -c "$PROJECT_DIR/RTT/SEGGER_RTT_ASM_ARMv7M.S" \
    -o "$build_dir/SEGGER_RTT_ASM_ARMv7M.o"

  "$AR" rcs "$build_dir/libarm_segger_rtt.a" \
    "$build_dir/SEGGER_RTT.o" \
    "$build_dir/rtt_printf.o" \
    "$build_dir/rtt_log.o" \
    "$build_dir/rtt_core.o" \
    "$build_dir/SEGGER_RTT_ASM_ARMv7M.o"

  "$CC" "$@" -"$optimization" -std=c11 -ffunction-sections \
    -fdata-sections -Wall -Wextra -Werror \
    -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
    -c "$SCRIPT_DIR/rtt_log_arm_probe.c" -o "$build_dir/probe.o"

  "$CC" "$@" -"$optimization" --specs=nosys.specs -nostartfiles \
    -Wl,-e,main -Wl,--gc-sections \
    "$build_dir/probe.o" "$build_dir/libarm_segger_rtt.a" \
    -lm -lc -lnosys -o "$build_dir/probe.elf"
}

for optimization in O0 Os; do
  build_one m0 "$optimization" -mcpu=cortex-m0 -mthumb
  build_one m7 "$optimization" -mcpu=cortex-m7 -mthumb \
    -mfpu=fpv5-d16 -mfloat-abi=hard
done
