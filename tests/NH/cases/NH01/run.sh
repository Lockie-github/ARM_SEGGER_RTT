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
  BUILD_DIR=$(mktemp -d "${TMPDIR:-/tmp}/rtt-log-test.XXXXXX")
  KEEP_BUILD=0
fi

cleanup() {
  if [ "$KEEP_BUILD" -eq 0 ]; then
    rm -rf "$BUILD_DIR"
  fi
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
    "$PROJECT_DIR/rtt_float.c" \
    "$SCRIPT_DIR/rtt_log_test.c" \
    -o "$output"
  "$output"
}

expect_compile_failure() {
  name=$1
  shift
  log="$BUILD_DIR/rtt_log_test_$name.log"
  if "$CC" -std=c11 -Wall -Wextra -Werror -pedantic \
      "$@" \
      -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
      -fsyntax-only "$PROJECT_DIR/rtt_log.c" >"$log" 2>&1; then
    printf 'Expected configuration to fail compilation: %s\n' "$name" >&2
    return 1
  fi
  if ! grep -Fq \
      'RTT_LOG_BUFFER_INDEX must be less than SEGGER_RTT_MAX_NUM_UP_BUFFERS' \
      "$log"; then
    printf 'Configuration failed for an unexpected reason: %s\n' "$name" >&2
    sed -n '1,120p' "$log" >&2
    return 1
  fi
}

for lite in 0 1; do
  for color in 0 1; do
    build_and_run "lite_${lite}_color_$color" \
      -DLOG_ENABLE_LITE="$lite" -DRTT_LOG_USE_COLOR="$color" \
      -DRTT_LOG_BUFFER_INDEX=2u
  done
done

build_and_run "disabled" -DRTT_LOG_ENABLE=0
build_and_run "disabled_invalid_buffer_index" \
  -DRTT_LOG_ENABLE=0 -DRTT_LOG_BUFFER_INDEX=3u \
  -DSEGGER_RTT_MAX_NUM_UP_BUFFERS=1
build_and_run "lite_info_disabled" \
  -DLOG_ENABLE_LITE=1 -DLOG_ENABLE_INFO=0
for level in INFO DEBUG WARN ERROR PRINT STRING FLOAT; do
  build_and_run "${level}_disabled" "-DLOG_ENABLE_${level}=0"
done
build_and_run "individual_disabled" \
  -DLOG_ENABLE_INFO=0 -DLOG_ENABLE_DEBUG=0 -DLOG_ENABLE_WARN=0 \
  -DLOG_ENABLE_ERROR=0 -DLOG_ENABLE_PRINT=0 -DLOG_ENABLE_STRING=0 \
  -DLOG_ENABLE_FLOAT=0
build_and_run "config_override" -DRTT_TEST_CONFIG_OVERRIDE=1 \
  -I"$SCRIPT_DIR/configs/override"
build_and_run "modff" -DRTT_FLOAT_USE_MODFF=1 -lm
expect_compile_failure "buffer_index_equal_count" \
  -DRTT_LOG_BUFFER_INDEX=1u -DSEGGER_RTT_MAX_NUM_UP_BUFFERS=1
expect_compile_failure "buffer_index_above_count" \
  -DRTT_LOG_BUFFER_INDEX=3u -DSEGGER_RTT_MAX_NUM_UP_BUFFERS=1

sh "$SCRIPT_DIR/quality.sh" "$BUILD_DIR/quality"
