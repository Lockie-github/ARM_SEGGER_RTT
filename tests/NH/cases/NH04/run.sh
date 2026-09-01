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
RELEASE_COMMIT=$(git -C "$PROJECT_DIR" rev-parse release)

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

mkdir -p "$BUILD_DIR/release"
git -C "$PROJECT_DIR" archive "$RELEASE_COMMIT" | tar -x -C "$BUILD_DIR/release"

"$CC" -std=c11 -Wall -Wextra -Werror -pedantic -DRTT_COMPARE_RELEASE \
  -I"$BUILD_DIR/release" -I"$BUILD_DIR/release/RTT" \
  "$BUILD_DIR/release/RTT/SEGGER_RTT_printf.c" \
  "$SCRIPT_DIR/../../support/fixtures/rtt_printf_compare.c" \
  -o "$BUILD_DIR/release_compare"
printf 'release@%s: ' "$RELEASE_COMMIT"
"$BUILD_DIR/release_compare"

"$CC" -std=c11 -Wall -Wextra -Werror -pedantic \
  -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
  "$PROJECT_DIR/rtt_printf.c" \
  "$SCRIPT_DIR/../../support/fixtures/rtt_printf_compare.c" \
  -o "$BUILD_DIR/current_compare"
printf 'current-worktree: '
"$BUILD_DIR/current_compare"

printf '%s\n' 'NH04 PASS comparison: common outputs and long-message writes matched'
