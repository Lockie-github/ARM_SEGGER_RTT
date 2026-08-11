#!/bin/sh
set -eu

CC=${CC:-cc}
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
BUILD_DIR=$(mktemp -d "${TMPDIR:-/tmp}/rtt-log-test.XXXXXX")

cleanup() {
  rm -rf "$BUILD_DIR"
}
trap cleanup EXIT INT TERM

for lite in 0 1; do
  for color in 0 1; do
    output="$BUILD_DIR/rtt_log_test_lite_${lite}_color_$color"
    "$CC" -std=c11 -Wall -Wextra -Werror -pedantic \
      -DLOG_ENABLE_LITE="$lite" -DRTT_LOG_USE_COLOR="$color" \
      -DRTT_LOG_BUFFER_INDEX=2u \
      -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
      "$PROJECT_DIR/RTT/rtt_printf.c" \
      "$PROJECT_DIR/RTT/rtt_log.c" \
      "$SCRIPT_DIR/rtt_log_test.c" \
      -o "$output"
    "$output"
  done
done
