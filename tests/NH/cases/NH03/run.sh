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
  BUILD_DIR=$(mktemp -d "${TMPDIR:-/tmp}/rtt-nh03-test.XXXXXX")
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
  "$CC" -std=c11 -Wall -Wextra -Werror -pedantic \
    "$@" \
    -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
    "$PROJECT_DIR/rtt_printf.c" \
    "$PROJECT_DIR/rtt_log.c" \
    "$SCRIPT_DIR/rtt_log_simple_api_test.c" \
    -o "$BUILD_DIR/$name"
  "$BUILD_DIR/$name"
}

build_and_run enabled
build_and_run print_disabled -DLOG_ENABLE_PRINT=0
build_and_run string_disabled -DLOG_ENABLE_STRING=0
