#!/bin/sh
set -eu

HOST_CC=${HOST_CC:-cc}
HOST_NM=${HOST_NM:-nm}
TOOLCHAIN_PREFIX=${TOOLCHAIN_PREFIX:-arm-none-eabi-}
ARM_CC=${TOOLCHAIN_PREFIX}gcc
ARM_NM=${TOOLCHAIN_PREFIX}nm
ARM_OBJDUMP=${TOOLCHAIN_PREFIX}objdump
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/../../../.." && pwd)
if [ -z "${HOST_GC_LDFLAGS:-}" ]; then
  case $(uname -s) in
    Darwin) HOST_GC_LDFLAGS=-Wl,-dead_strip ;;
    *) HOST_GC_LDFLAGS=-Wl,--gc-sections ;;
  esac
fi

if [ -n "${NH11_OUTPUT_DIR:-}" ]; then
  BUILD_ROOT=$NH11_OUTPUT_DIR
  mkdir -p "$BUILD_ROOT/host" "$BUILD_ROOT/arm"
  KEEP_BUILD=1
else
  BUILD_ROOT=$(mktemp -d "${TMPDIR:-/tmp}/rtt-nh11-test.XXXXXX")
  mkdir -p "$BUILD_ROOT/host" "$BUILD_ROOT/arm"
  KEEP_BUILD=0
fi

cleanup() {
  if [ "$KEEP_BUILD" -eq 0 ]; then
    rm -rf "$BUILD_ROOT"
  fi
}
trap cleanup EXIT INT TERM

COMMON_DEFINES="-DLOG_ENABLE_TYPED=1 -DLOG_ENABLE_FLOAT=0 -DLOG_ENABLE_TYPED_FLOAT=0 -DRTT_LOG_BUFFER_INDEX=1"

build_host_profile() {
  name=$1
  lite=$2
  color=$3
  binary="$BUILD_ROOT/host/$name"

  # shellcheck disable=SC2086
  "$HOST_CC" -std=c11 -Wall -Wextra -Werror -pedantic \
    -ffunction-sections -fdata-sections $COMMON_DEFINES \
    -DLOG_ENABLE_LITE="$lite" -DRTT_LOG_USE_COLOR="$color" \
    -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
    "$PROJECT_DIR/rtt_log.c" "$SCRIPT_DIR/rtt_typed_nh11_test.c" \
    "$HOST_GC_LDFLAGS" -o "$binary" \
    >"$BUILD_ROOT/host/$name.build.log" 2>&1
  "$binary" >"$BUILD_ROOT/host/$name.tsv" \
    2>"$BUILD_ROOT/host/$name.stderr.log"
  "$HOST_NM" "$binary" >"$BUILD_ROOT/host/$name.symbols.txt"
}

build_host_profile lite0_color0 0 0
build_host_profile lite0_color1 0 1
build_host_profile lite1_color0 1 0
build_host_profile lite1_color1 1 1

cmp "$BUILD_ROOT/host/lite0_color0.tsv" "$BUILD_ROOT/host/lite0_color1.tsv"
cmp "$BUILD_ROOT/host/lite0_color0.tsv" "$BUILD_ROOT/host/lite1_color0.tsv"
cmp "$BUILD_ROOT/host/lite0_color0.tsv" "$BUILD_ROOT/host/lite1_color1.tsv"

for profile in lite0_color0 lite0_color1 lite1_color0 lite1_color1; do
  if grep -Eq 'RTT_vprintfFramed|SEGGER_RTT_WriteString' \
      "$BUILD_ROOT/host/$profile.symbols.txt"; then
    printf 'NH11 %s retained formatter or WriteString in final binary\n' \
      "$profile" >&2
    exit 1
  fi
done

# Product-object dependency inspection is separate from the host test harness,
# which legitimately uses libc for its oracle and evidence formatting.
# shellcheck disable=SC2086
"$HOST_CC" -std=c11 -Wall -Wextra -Werror -pedantic \
  -ffunction-sections -fdata-sections $COMMON_DEFINES \
  -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
  -c "$PROJECT_DIR/rtt_log.c" -o "$BUILD_ROOT/host/rtt_log.o"
"$HOST_NM" -u "$BUILD_ROOT/host/rtt_log.o" \
  >"$BUILD_ROOT/host/rtt_log.undefined.txt"
if grep -Eq '(^|[[:space:]_])(malloc|calloc|realloc|free)$' \
    "$BUILD_ROOT/host/rtt_log.undefined.txt"; then
  printf '%s\n' 'NH11 typed product object unexpectedly references heap allocation' >&2
  exit 1
fi

M0_FLAGS="-mcpu=cortex-m0 -mthumb"
# shellcheck disable=SC2086
"$ARM_CC" $M0_FLAGS -Os -std=c11 -ffreestanding \
  -ffunction-sections -fdata-sections -Wall -Wextra -Werror \
  $COMMON_DEFINES -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
  -c "$SCRIPT_DIR/rtt_typed_nh11_arm_probe.c" \
  -o "$BUILD_ROOT/arm/probe.o" \
  >"$BUILD_ROOT/arm/build.log" 2>&1
# shellcheck disable=SC2086
"$ARM_CC" $M0_FLAGS -Os -nostdlib -Wl,-e,main -Wl,--gc-sections \
  -Wl,-Map="$BUILD_ROOT/arm/result.map" "$BUILD_ROOT/arm/probe.o" \
  -o "$BUILD_ROOT/arm/result.elf" >>"$BUILD_ROOT/arm/build.log" 2>&1
"$ARM_NM" "$BUILD_ROOT/arm/result.elf" >"$BUILD_ROOT/arm/symbols.txt"
"$ARM_NM" -u "$BUILD_ROOT/arm/result.elf" >"$BUILD_ROOT/arm/undefined.txt"
"$ARM_OBJDUMP" -d "$BUILD_ROOT/arm/result.elf" \
  >"$BUILD_ROOT/arm/disassembly.txt"

for symbol in RTT_LogI32 RTT_LogU32 RTT_LogHex32 RTT_LogPointer; do
  if ! grep -Eq "[[:space:]]$symbol$" "$BUILD_ROOT/arm/symbols.txt"; then
    printf 'NH11 Arm ELF missing %s\n' "$symbol" >&2
    exit 1
  fi
done
if [ -s "$BUILD_ROOT/arm/undefined.txt" ]; then
  printf '%s\n' 'NH11 Arm ELF has unresolved symbols' >&2
  sed -n '1,80p' "$BUILD_ROOT/arm/undefined.txt" >&2
  exit 1
fi
if grep -Eq 'RTT_LogPrintf|RTT_vprintfFramed|malloc|calloc|realloc|free|__aeabi_.*div|__.[u]?divsi3' \
    "$BUILD_ROOT/arm/symbols.txt"; then
  printf '%s\n' 'NH11 Arm ELF retained forbidden formatter/heap/division dependency' >&2
  exit 1
fi

printf '%s\n' 'NH11 PASS: 4 host profiles produced identical typed transcripts'
printf '%s\n' 'NH11 PASS: 64-bit host values, labels, one-write and error returns passed'
printf '%s\n' 'NH11 PASS: Cortex-M0 final ELF validated 32-bit pointer width and dependencies'
