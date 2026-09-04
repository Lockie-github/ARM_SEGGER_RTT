#!/bin/sh
set -eu

CC=${CC:-cc}
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/../../../.." && pwd)
if [ -n "${NH_OUTPUT_DIR:-}" ]; then
  BUILD_DIR=$NH_OUTPUT_DIR
  mkdir -p "$BUILD_DIR"
  KEEP_BUILD=1
else
  BUILD_DIR=$(mktemp -d "${TMPDIR:-/tmp}/rtt-nh08-test.XXXXXX")
  KEEP_BUILD=0
fi

cleanup() {
  if [ "$KEEP_BUILD" -eq 0 ]; then
    rm -rf "$BUILD_DIR"
  fi
}
trap cleanup EXIT INT TERM

COMMON_FLAGS="-std=c11 -Wall -Wextra -Werror -pedantic"

# shellcheck disable=SC2086
"$CC" $COMMON_FLAGS -DLOG_ENABLE_LITE=1 -DRTT_LOG_USE_COLOR=0 \
  -DRTT_FLOAT_USE_MODFF=0 \
  -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
  "$PROJECT_DIR/rtt_printf.c" \
  "$PROJECT_DIR/rtt_log.c" \
  "$PROJECT_DIR/rtt_float.c" \
  "$SCRIPT_DIR/rtt_write_failure_nh08_test.c" \
  -o "$BUILD_DIR/write_failure"
"$BUILD_DIR/write_failure"

# shellcheck disable=SC2086
"$CC" $COMMON_FLAGS -DRTT_USE_ASM=0 \
  -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
  "$PROJECT_DIR/RTT/SEGGER_RTT.c" \
  "$SCRIPT_DIR/rtt_write_nolock_nh08_test.c" \
  -o "$BUILD_DIR/write_nolock"
"$BUILD_DIR/write_nolock"

printf '%s\n' 'NH08 PASS: write failures, short writes, stop-on-error, recovery, C ring buffer'
