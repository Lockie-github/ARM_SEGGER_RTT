#!/bin/sh
set -eu

CC=${CC:-cc}
BASELINE_REF=${NHT_FMT_COMPAT_BASELINE_REF:-release}
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/../../../.." && pwd)
if [ -n "${NH_OUTPUT_DIR:-}" ]; then
  BUILD_DIR=$NH_OUTPUT_DIR
  mkdir -p "$BUILD_DIR"
  KEEP_BUILD=1
else
  BUILD_DIR=$(mktemp -d "${TMPDIR:-/tmp}/rtt-nht-fmt-compat.XXXXXX")
  KEEP_BUILD=0
fi
BASELINE_COMMIT=$(git -C "$PROJECT_DIR" rev-parse --verify "${BASELINE_REF}^{commit}")

cleanup() {
  if [ "$KEEP_BUILD" -eq 0 ]; then
    rm -rf "$BUILD_DIR"
  fi
}
trap cleanup EXIT INT TERM

printf '%s\n' "$BASELINE_COMMIT" >"$BUILD_DIR/baseline_commit.txt"
mkdir -p "$BUILD_DIR/baseline"
git -C "$PROJECT_DIR" archive "$BASELINE_COMMIT" | tar -x -C "$BUILD_DIR/baseline"

"$CC" -std=c11 -Wall -Wextra -Werror -pedantic -DRTT_COMPARE_BASELINE \
  -I"$BUILD_DIR/baseline" -I"$BUILD_DIR/baseline/RTT" \
  "$BUILD_DIR/baseline/RTT/SEGGER_RTT_printf.c" \
  "$SCRIPT_DIR/rtt_printf_compare.c" \
  -o "$BUILD_DIR/baseline_compare"
printf 'baseline=%s@%s: ' "$BASELINE_REF" "$BASELINE_COMMIT"
"$BUILD_DIR/baseline_compare"

"$CC" -std=c11 -Wall -Wextra -Werror -pedantic \
  -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
  "$PROJECT_DIR/rtt_printf.c" \
  "$SCRIPT_DIR/rtt_printf_compare.c" \
  -o "$BUILD_DIR/current_compare"
printf 'current-worktree: '
"$BUILD_DIR/current_compare"

printf '%s\n' 'NHT_FMT_COMPAT PASS: common outputs and long-message behavior matched the compatibility expectations'
