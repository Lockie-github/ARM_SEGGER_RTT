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

build_and_run() {
  name=$1
  shift
  output="$BUILD_DIR/rtt_log_test_$name"
  "$CC" -std=c11 -Wall -Wextra -Werror -pedantic \
    "$@" \
    -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
    "$PROJECT_DIR/rtt_printf.c" \
    "$PROJECT_DIR/rtt_log.c" \
    "$PROJECT_DIR/rtt_core.c" \
    "$SCRIPT_DIR/rtt_log_test.c" \
    -o "$output"
  "$output"
}

for lite in 0 1; do
  for color in 0 1; do
    build_and_run "lite_${lite}_color_$color" \
      -DLOG_ENABLE_LITE="$lite" -DRTT_LOG_USE_COLOR="$color" \
      -DRTT_LOG_BUFFER_INDEX=2u
  done
done

build_and_run "disabled" -DRTT_LOG_ENABLE=0
build_and_run "lite_info_disabled" \
  -DLOG_ENABLE_LITE=1 -DLOG_ENABLE_INFO=0
for level in INFO DEBUG WARN ERROR PRINT FLOAT; do
  build_and_run "${level}_disabled" "-DLOG_ENABLE_${level}=0"
done
build_and_run "individual_disabled" \
  -DLOG_ENABLE_INFO=0 -DLOG_ENABLE_DEBUG=0 -DLOG_ENABLE_WARN=0 \
  -DLOG_ENABLE_ERROR=0 -DLOG_ENABLE_PRINT=0 -DLOG_ENABLE_FLOAT=0
build_and_run "config_override" -I"$SCRIPT_DIR/config_override"
build_and_run "fpu" -DHAS_FPU=1 -lm
