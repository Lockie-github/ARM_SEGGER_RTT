#!/bin/sh
set -eu

CC=${CC:-cc}
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/../../../.." && pwd)
HOST_NM=${HOST_NM:-nm}
if [ -n "${NH_OUTPUT_DIR:-}" ]; then
  BUILD_DIR=$NH_OUTPUT_DIR
  mkdir -p "$BUILD_DIR"
  KEEP_BUILD=1
else
  BUILD_DIR=$(mktemp -d "${TMPDIR:-/tmp}/rtt-nh05-test.XXXXXX")
  KEEP_BUILD=0
fi

cleanup() {
  if [ "$KEEP_BUILD" -eq 0 ]; then
    rm -rf "$BUILD_DIR"
  fi
}
trap cleanup EXIT INT TERM

build_enabled() {
  name=$1
  use_modff=$2
  shift 2
  "$CC" -std=c11 -Wall -Wextra -Werror -pedantic \
    -DRTT_FLOAT_USE_MODFF="$use_modff" \
    -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
    "$PROJECT_DIR/rtt_printf.c" \
    "$PROJECT_DIR/rtt_log.c" \
    "$PROJECT_DIR/rtt_float.c" \
    "$SCRIPT_DIR/rtt_float_nh05_test.c" \
    -o "$BUILD_DIR/$name" "$@"
  "$BUILD_DIR/$name" >"$BUILD_DIR/$name.out"
}

build_disabled() {
  name=$1
  shift
  "$CC" -std=c11 -Wall -Wextra -Werror -pedantic \
    -DRTT_FLOAT_USE_MODFF=1 "$@" \
    -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
    "$PROJECT_DIR/rtt_printf.c" \
    "$PROJECT_DIR/rtt_log.c" \
    "$PROJECT_DIR/rtt_float.c" \
    "$SCRIPT_DIR/rtt_float_disabled_nh05_test.c" \
    -o "$BUILD_DIR/$name"
  "$BUILD_DIR/$name"
  if "$HOST_NM" -u "$BUILD_DIR/$name" | grep -q 'modff'; then
    printf 'NH05 %s unexpectedly references modff\n' "$name" >&2
    return 1
  fi
}

build_enabled soft 0
build_enabled hard 1 -lm
cmp "$BUILD_DIR/soft.out" "$BUILD_DIR/hard.out"
sed -n '1,120p' "$BUILD_DIR/soft.out"

if "$HOST_NM" -u "$BUILD_DIR/soft" | grep -q 'modff'; then
  printf '%s\n' 'NH05 soft path unexpectedly references modff' >&2
  exit 1
fi

build_disabled float_disabled -DLOG_ENABLE_FLOAT=0
build_disabled log_disabled -DRTT_LOG_ENABLE=0

legacy_log="$BUILD_DIR/legacy-hard-fpu.log"
if "$CC" -std=c11 -Wall -Wextra -Werror -pedantic \
    -DHARD_FPU_ENABLE=1 \
    -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
    -fsyntax-only "$PROJECT_DIR/rtt_float.c" >"$legacy_log" 2>&1; then
  printf '%s\n' 'NH05 legacy HARD_FPU_ENABLE unexpectedly compiled' >&2
  exit 1
fi
if ! grep -Fq \
    'HARD_FPU_ENABLE was renamed to RTT_FLOAT_USE_MODFF' "$legacy_log"; then
  printf '%s\n' 'NH05 legacy HARD_FPU_ENABLE failed for an unexpected reason' >&2
  sed -n '1,120p' "$legacy_log" >&2
  exit 1
fi

printf '%s\n' 'NH05 PASS comparison: RTT_FLOAT_USE_MODFF=0/1 transcripts matched'
printf '%s\n' 'NH05 PASS dependencies: soft/disabled builds have no modff reference'
printf '%s\n' 'NH05 PASS migration: HARD_FPU_ENABLE rejected with rename diagnostic'
