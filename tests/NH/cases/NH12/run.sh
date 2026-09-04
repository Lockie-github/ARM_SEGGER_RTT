#!/bin/sh
set -eu

HOST_CC=${HOST_CC:-cc}
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/../../../.." && pwd)

if [ -n "${NH12_OUTPUT_DIR:-}" ]; then
  BUILD_ROOT=$NH12_OUTPUT_DIR
  mkdir -p "$BUILD_ROOT/failure" "$BUILD_ROOT/corpus"
  KEEP_BUILD=1
else
  BUILD_ROOT=$(mktemp -d "${TMPDIR:-/tmp}/rtt-nh12-test.XXXXXX")
  mkdir -p "$BUILD_ROOT/failure" "$BUILD_ROOT/corpus"
  KEEP_BUILD=0
fi

cleanup() {
  if [ "$KEEP_BUILD" -eq 0 ]; then
    rm -rf "$BUILD_ROOT"
  fi
}
trap cleanup EXIT INT TERM

COMMON="-std=c11 -Wall -Wextra -Werror -pedantic -DLOG_ENABLE_LITE=1 -DRTT_LOG_USE_COLOR=0 -DLOG_ENABLE_TYPED=1 -DLOG_ENABLE_TYPED_FLOAT=1 -DLOG_ENABLE_FLOAT=1 -DRTT_LOG_BUFFER_INDEX=1"

# shellcheck disable=SC2086
"$HOST_CC" $COMMON -DSEGGER_RTT_PRINTF_BUFFER_SIZE=64 \
  -DRTT_FLOAT_USE_MODFF=0 -DRTT_LOG_FLOAT_FAST_PATH=0 \
  -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
  "$PROJECT_DIR/rtt_printf.c" "$PROJECT_DIR/rtt_log.c" \
  "$PROJECT_DIR/rtt_float.c" "$SCRIPT_DIR/rtt_failure_nh12_test.c" \
  -o "$BUILD_ROOT/failure/nh12_failure" \
  >"$BUILD_ROOT/failure/build.log" 2>&1
"$BUILD_ROOT/failure/nh12_failure" \
  >"$BUILD_ROOT/failure/results.tsv" \
  2>"$BUILD_ROOT/failure/stderr.log"

build_corpus() {
  name=$1
  modff=$2
  fast=$3
  # shellcheck disable=SC2086
  "$HOST_CC" $COMMON -DRTT_FLOAT_USE_MODFF="$modff" \
    -DRTT_LOG_FLOAT_FAST_PATH="$fast" \
    -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
    "$PROJECT_DIR/rtt_printf.c" "$PROJECT_DIR/rtt_log.c" \
    "$PROJECT_DIR/rtt_float.c" "$SCRIPT_DIR/rtt_float_nh12_corpus.c" \
    -o "$BUILD_ROOT/corpus/$name" -lm \
    >"$BUILD_ROOT/corpus/$name.build.log" 2>&1
  "$BUILD_ROOT/corpus/$name" random \
    >"$BUILD_ROOT/corpus/$name.random.tsv" \
    2>"$BUILD_ROOT/corpus/$name.random.stderr.log"
  shasum -a 256 "$BUILD_ROOT/corpus/$name.random.tsv" \
    >"$BUILD_ROOT/corpus/$name.random.sha256"
}

build_corpus bit_fast0 0 0
build_corpus bit_fast1 0 1
build_corpus modff_fast0 1 0
build_corpus modff_fast1 1 1

cmp "$BUILD_ROOT/corpus/bit_fast0.random.tsv" \
  "$BUILD_ROOT/corpus/bit_fast1.random.tsv"
cmp "$BUILD_ROOT/corpus/bit_fast0.random.tsv" \
  "$BUILD_ROOT/corpus/modff_fast0.random.tsv"
cmp "$BUILD_ROOT/corpus/bit_fast0.random.tsv" \
  "$BUILD_ROOT/corpus/modff_fast1.random.tsv"

label_failures=0
for name in bit_fast0 bit_fast1 modff_fast0 modff_fast1; do
  set +e
  "$BUILD_ROOT/corpus/$name" labels \
    >"$BUILD_ROOT/corpus/$name.labels.tsv" \
    2>"$BUILD_ROOT/corpus/$name.labels.stderr.log"
  label_rc=$?
  set -e
  printf '%s\n' "$label_rc" >"$BUILD_ROOT/corpus/$name.labels.exit"
  if [ "$label_rc" -ne 0 ]; then
    label_failures=$((label_failures + 1))
  fi
done

cmp "$BUILD_ROOT/corpus/bit_fast0.labels.tsv" \
  "$BUILD_ROOT/corpus/modff_fast0.labels.tsv"

printf '%s\n' 'NH12 PASS: 13 API write matrix and formatter flush failures'
printf '%s\n' 'NH12 PASS: 100000 random binary32 plus fixed numeric corpus'
printf '%s\n' 'NH12 PASS: modff 0/1 and fast 0/1 numeric transcripts matched'
if [ "$label_failures" -ne 0 ]; then
  printf 'NH12 FAIL: %u fast-path label corpus run(s) aborted\n' \
    "$label_failures" >&2
  exit 1
fi
printf '%s\n' 'NH12 PASS: normalized label transcripts matched'
