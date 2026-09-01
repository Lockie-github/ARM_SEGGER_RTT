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
  BUILD_DIR=$(mktemp -d "${TMPDIR:-/tmp}/rtt-nh02-test.XXXXXX")
  KEEP_BUILD=0
fi

cleanup() {
  if [ "$KEEP_BUILD" -eq 0 ]; then
    rm -rf "$BUILD_DIR"
  fi
}
trap cleanup EXIT INT TERM

"$CC" -std=c11 -Wall -Wextra -Werror -pedantic \
  -DLOG_ENABLE_LITE=0 -DRTT_LOG_USE_COLOR=1 \
  -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
  "$PROJECT_DIR/rtt_printf.c" \
  "$PROJECT_DIR/rtt_log.c" \
  "$SCRIPT_DIR/rtt_log_levels_test.c" \
  -o "$BUILD_DIR/rtt_log_levels_test"

"$BUILD_DIR/rtt_log_levels_test"
