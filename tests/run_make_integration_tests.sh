#!/bin/sh
set -eu

TOOLCHAIN_PREFIX=${TOOLCHAIN_PREFIX:-/opt/arm-gnu-toolchain-12.2.rel1-darwin-arm64-arm-none-eabi/bin/arm-none-eabi-}
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
PROJECT_PARENT=$(CDPATH= cd -- "$PROJECT_DIR/.." && pwd)
BUILD_DIR=$(mktemp -d "${TMPDIR:-/tmp}/rtt-make-test.XXXXXX")

cleanup() {
  rm -rf "$BUILD_DIR"
}
trap cleanup EXIT INT TERM

make -C "$PROJECT_PARENT" \
  -f ARM_SEGGER_RTT/tests/make_asm_build.mk \
  BUILD_DIR="$BUILD_DIR" \
  AS="${TOOLCHAIN_PREFIX}gcc" \
  all

test -f "$BUILD_DIR/SEGGER_RTT_ASM_ARMv7M.o"
