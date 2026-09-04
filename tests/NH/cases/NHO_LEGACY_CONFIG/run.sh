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
  BUILD_DIR=$(mktemp -d "${TMPDIR:-/tmp}/rtt-nho-legacy-config.XXXXXX")
  KEEP_BUILD=0
fi

cleanup() {
  if [ "$KEEP_BUILD" -eq 0 ]; then
    rm -rf "$BUILD_DIR"
  fi
}
trap cleanup EXIT INT TERM

legacy_log="$BUILD_DIR/legacy-hard-fpu.log"
if "$CC" -std=c11 -Wall -Wextra -Werror -pedantic \
    -DHARD_FPU_ENABLE=1 \
    -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
    -fsyntax-only "$PROJECT_DIR/rtt_float.c" >"$legacy_log" 2>&1; then
  printf '%s\n' 'NHO_LEGACY_CONFIG legacy HARD_FPU_ENABLE unexpectedly compiled' >&2
  exit 1
fi
if ! grep -Fq \
    'HARD_FPU_ENABLE was renamed to RTT_FLOAT_USE_MODFF' "$legacy_log"; then
  printf '%s\n' 'NHO_LEGACY_CONFIG legacy HARD_FPU_ENABLE failed for an unexpected reason' >&2
  sed -n '1,120p' "$legacy_log" >&2
  exit 1
fi

printf '%s\n' 'NHO_LEGACY_CONFIG PASS: HARD_FPU_ENABLE rejected with rename diagnostic'
