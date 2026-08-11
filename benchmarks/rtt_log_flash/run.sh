#!/bin/sh
set -eu

TOOLCHAIN_PREFIX=${TOOLCHAIN_PREFIX:-/opt/arm-gnu-toolchain-12.2.rel1-darwin-arm64-arm-none-eabi/bin/arm-none-eabi-}
CC=${TOOLCHAIN_PREFIX}gcc
SIZE=${TOOLCHAIN_PREFIX}size
GIT=${GIT:-git}
BASELINE_REF=${BASELINE_REF:-dadde11}
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/../.." && pwd)
BUILD_DIR=$(mktemp -d "${TMPDIR:-/tmp}/rtt-log-flash.XXXXXX")

cleanup() {
  rm -rf "$BUILD_DIR"
}
trap cleanup EXIT INT TERM

common_flags="-mcpu=cortex-m0 -mthumb -std=gnu11 -ffunction-sections -fdata-sections -fno-lto -Wall -Wextra -Werror"

object_flash() {
  "$SIZE" "$@" | awk 'NR > 1 { text += $1; data += $2 } END { print text + data }'
}

printf 'optimization,count,direct_flash,collected_flash,saving,saving_percent,formatter_delta\n'
for optimization in O0 Os; do
  collected_impl="$BUILD_DIR/rtt_log_collected_${optimization}.o"
  old_formatter="$BUILD_DIR/rtt_printf_old_${optimization}.o"
  new_formatter="$BUILD_DIR/rtt_printf_new_${optimization}.o"
  "$CC" $common_flags -"$optimization" \
    -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
    -c "$PROJECT_DIR/RTT/rtt_log.c" -o "$collected_impl"
  "$GIT" -C "$PROJECT_DIR" show "$BASELINE_REF:RTT/rtt_printf.c" | \
    "$CC" $common_flags -"$optimization" \
      -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
      -x c -c -o "$old_formatter" -
  "$CC" $common_flags -"$optimization" \
    -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
    -c "$PROJECT_DIR/RTT/rtt_printf.c" -o "$new_formatter"
  formatter_delta=$(($(object_flash "$new_formatter") - $(object_flash "$old_formatter")))

  for count in 10 50 100; do
    direct="$BUILD_DIR/direct_${optimization}_${count}.o"
    collected="$BUILD_DIR/collected_${optimization}_${count}.o"

    "$CC" $common_flags -"$optimization" -DBENCH_COUNT="$count" \
      -c "$SCRIPT_DIR/benchmark.c" -o "$direct"
    "$CC" $common_flags -"$optimization" -DBENCH_COUNT="$count" \
      -DBENCH_COLLECTED=1 -c "$SCRIPT_DIR/benchmark.c" -o "$collected"

    direct_flash=$(object_flash "$direct")
    collected_flash=$(($(object_flash "$collected" "$collected_impl") + formatter_delta))
    saving=$((direct_flash - collected_flash))
    saving_percent=$(awk -v saving="$saving" -v direct="$direct_flash" \
      'BEGIN { printf "%.2f", (saving * 100.0) / direct }')

    printf '%s,%s,%s,%s,%s,%s%%,%s\n' \
      "$optimization" "$count" "$direct_flash" "$collected_flash" \
      "$saving" "$saving_percent" "$formatter_delta"
  done
done
