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
  BUILD_DIR=$(mktemp -d "${TMPDIR:-/tmp}/rtt-nh04-test.XXXXXX")
  KEEP_BUILD=0
fi
cleanup() {
  if [ "$KEEP_BUILD" -eq 0 ]; then
    rm -rf "$BUILD_DIR"
  fi
}
trap cleanup EXIT INT TERM

"$CC" -std=c11 -Wall -Wextra -Werror -pedantic \
  -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
  "$PROJECT_DIR/rtt_printf.c" \
  "$SCRIPT_DIR/rtt_formatter_nh04_test.c" \
  -o "$BUILD_DIR/current"
"$BUILD_DIR/current"

"$CC" -std=c11 -Wall -Wextra -Werror -pedantic \
  -DSEGGER_RTT_PRINTF_BUFFER_SIZE=8u -DNH04_SMALL_BUFFER_ONLY \
  -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
  "$PROJECT_DIR/rtt_printf.c" \
  "$SCRIPT_DIR/rtt_formatter_nh04_test.c" \
  -o "$BUILD_DIR/current_buffer_8"
"$BUILD_DIR/current_buffer_8"
