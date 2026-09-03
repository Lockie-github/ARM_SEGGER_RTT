#!/bin/sh
set -eu

LC_ALL=C
export LC_ALL

TOOLCHAIN_PREFIX=${TOOLCHAIN_PREFIX:-arm-none-eabi-}
BASELINE_REF=${NHT_FLASH_OPT_BASELINE_REF:-release}
ARM_CC=${TOOLCHAIN_PREFIX}gcc
ARM_SIZE=${TOOLCHAIN_PREFIX}size
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/../../../.." && pwd)
OUTPUT_DIR=$1
WORK_ROOT=$(mktemp -d "${TMPDIR:-/tmp}/rtt-nht-flash-release.XXXXXX")
RELEASE_DIR="$WORK_ROOT/release"

cleanup() {
  rm -rf "$WORK_ROOT"
}
trap cleanup EXIT INT TERM

if test -e "$OUTPUT_DIR"; then
  printf 'NHT_FLASH_OPT release comparison output already exists: %s\n' "$OUTPUT_DIR" >&2
  exit 1
fi
mkdir -p "$OUTPUT_DIR/artifacts" "$RELEASE_DIR"

BASELINE_COMMIT=$(git -C "$PROJECT_DIR" rev-parse --verify "${BASELINE_REF}^{commit}")
git -C "$PROJECT_DIR" archive "$BASELINE_COMMIT" | tar -x -C "$RELEASE_DIR"
for source in RTT/SEGGER_RTT.c RTT/SEGGER_RTT_printf.c RTT/SEGGER_RTT_ASM_ARMv7M.S; do
  if ! test -f "$RELEASE_DIR/$source"; then
    printf 'NHT_FLASH_OPT baseline %s lacks %s\n' "$BASELINE_COMMIT" "$source" >&2
    exit 1
  fi
done

printf '%s\n' "$BASELINE_REF" >"$OUTPUT_DIR/baseline_ref.txt"
printf '%s\n' "$BASELINE_COMMIT" >"$OUTPUT_DIR/baseline_commit.txt"
git -C "$PROJECT_DIR" rev-parse HEAD >"$OUTPUT_DIR/current_commit.txt"
"$ARM_CC" --version | sed -n '1p' >"$OUTPUT_DIR/toolchain.txt"
printf '%s\n' \
  'implementation,target,run,text,rodata,data,bss,flash,ram,elf_sha256' \
  >"$OUTPUT_DIR/measurements.csv"

COMMON_FLAGS='-std=gnu11 -Os -g0 -ffunction-sections -fdata-sections -fno-lto -Wall -Wextra -Werror'
COMMON_LINK='--specs=nosys.specs -nostartfiles -Wl,-e,main -Wl,--gc-sections'

section_value() {
  "$ARM_SIZE" -A "$1" | awk -v section="$2" \
    '$1 == section { total += $2 } END { print total + 0 }'
}

record_result() {
  implementation=$1
  target=$2
  run=$3
  build=$4
  elf="$build/result.elf"
  text=$(section_value "$elf" .text)
  rodata=$(section_value "$elf" .rodata)
  data=$(section_value "$elf" .data)
  bss=$(section_value "$elf" .bss)
  flash=$((text + rodata + data))
  ram=$((data + bss))
  sha=$(shasum -a 256 "$elf" | awk '{ print $1 }')
  printf '%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n' \
    "$implementation" "$target" "$run" "$text" "$rodata" "$data" "$bss" \
    "$flash" "$ram" "$sha" >>"$OUTPUT_DIR/measurements.csv"
  if test "$run" -eq 1; then
    cp "$elf" "$OUTPUT_DIR/artifacts/${implementation}_${target}.elf"
    cp "$build/result.map" "$OUTPUT_DIR/artifacts/${implementation}_${target}.map"
  fi
}

write_current_config() {
  config=$1
  {
    printf '%s\n' '#ifndef NHT_FLASH_OPT_RTT_CFG_H' '#define NHT_FLASH_OPT_RTT_CFG_H'
    printf '%s\n' '#define RTT_USER_CFG_ENABLE 1'
    printf '%s\n' '#define RTT_LOG_ENABLE 1' '#define LOG_ENABLE_LITE 0'
    printf '%s\n' '#define LOG_ENABLE_TYPED 0' '#define LOG_ENABLE_TYPED_FLOAT 0'
    printf '%s\n' '#define LOG_ENABLE_INFO 1' '#define LOG_ENABLE_DEBUG 1'
    printf '%s\n' '#define LOG_ENABLE_WARN 1' '#define LOG_ENABLE_ERROR 1'
    printf '%s\n' '#define LOG_ENABLE_PRINT 1' '#define LOG_ENABLE_STRING 1'
    printf '%s\n' '#define LOG_ENABLE_FLOAT 1' '#define RTT_FLOAT_USE_MODFF 0'
    printf '%s\n' '#define RTT_LOG_FLOAT_FAST_PATH 0' '#define RTT_WRITE_SKIP_USE_ASM 0'
    printf '%s\n' '#define RTT_LOG_BUFFER_INDEX 0u' '#define RTT_LOG_USE_COLOR 1'
    printf '%s\n' '#define SEGGER_RTT_MAX_NUM_UP_BUFFERS 3' '#define SEGGER_RTT_MAX_NUM_DOWN_BUFFERS 3'
    printf '%s\n' '#define BUFFER_SIZE_UP 1024' '#define BUFFER_SIZE_DOWN 16'
    printf '%s\n' '#define SEGGER_RTT_PRINTF_BUFFER_SIZE 64u' '#endif'
  } >"$config"
}

build_current() {
  target=$1
  run=$2
  arch_flags=$3
  build="$WORK_ROOT/current_${target}_${run}"
  mkdir -p "$build/config"
  write_current_config "$build/config/rtt_cfg.h"
  # shellcheck disable=SC2086
  "$ARM_CC" $COMMON_FLAGS $arch_flags -I"$build/config" -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
    -c "$SCRIPT_DIR/current_default_benchmark.c" -o "$build/benchmark.o"
  for source in RTT/SEGGER_RTT.c rtt_printf.c rtt_log.c rtt_float.c; do
    object=$(basename "$source" .c)
    # shellcheck disable=SC2086
    "$ARM_CC" $COMMON_FLAGS $arch_flags -I"$build/config" -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
      -c "$PROJECT_DIR/$source" -o "$build/$object.o"
  done
  # shellcheck disable=SC2086
  "$ARM_CC" $COMMON_FLAGS $arch_flags -x assembler-with-cpp \
    -I"$build/config" -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
    -c "$PROJECT_DIR/RTT/SEGGER_RTT_ASM_ARMv7M.S" -o "$build/SEGGER_RTT_ASM_ARMv7M.o"
  # shellcheck disable=SC2086
  "$ARM_CC" $COMMON_FLAGS $arch_flags $COMMON_LINK -Wl,-Map="$build/result.map" \
    "$build/benchmark.o" "$build/SEGGER_RTT.o" "$build/rtt_printf.o" \
    "$build/rtt_log.o" "$build/rtt_float.o" "$build/SEGGER_RTT_ASM_ARMv7M.o" \
    -lm -lc -lnosys -o "$build/result.elf"
  record_result current "$target" "$run" "$build"
}

build_release() {
  target=$1
  run=$2
  arch_flags=$3
  build="$WORK_ROOT/release_${target}_${run}"
  mkdir -p "$build"
  # shellcheck disable=SC2086
  "$ARM_CC" $COMMON_FLAGS $arch_flags -I"$RELEASE_DIR" -I"$RELEASE_DIR/RTT" \
    -c "$SCRIPT_DIR/release_default_benchmark.c" -o "$build/benchmark.o"
  # shellcheck disable=SC2086
  "$ARM_CC" $COMMON_FLAGS $arch_flags -I"$RELEASE_DIR" -I"$RELEASE_DIR/RTT" \
    -c "$RELEASE_DIR/RTT/SEGGER_RTT.c" -o "$build/SEGGER_RTT.o"
  # shellcheck disable=SC2086
  "$ARM_CC" $COMMON_FLAGS $arch_flags -I"$RELEASE_DIR" -I"$RELEASE_DIR/RTT" \
    -c "$RELEASE_DIR/RTT/SEGGER_RTT_printf.c" -o "$build/SEGGER_RTT_printf.o"
  # shellcheck disable=SC2086
  "$ARM_CC" $COMMON_FLAGS $arch_flags -x assembler-with-cpp \
    -I"$RELEASE_DIR" -I"$RELEASE_DIR/RTT" \
    -c "$RELEASE_DIR/RTT/SEGGER_RTT_ASM_ARMv7M.S" -o "$build/SEGGER_RTT_ASM_ARMv7M.o"
  # shellcheck disable=SC2086
  "$ARM_CC" $COMMON_FLAGS $arch_flags $COMMON_LINK -Wl,-Map="$build/result.map" \
    "$build/benchmark.o" "$build/SEGGER_RTT.o" "$build/SEGGER_RTT_printf.o" \
    "$build/SEGGER_RTT_ASM_ARMv7M.o" -lm -lc -lnosys -o "$build/result.elf"
  record_result release "$target" "$run" "$build"
}

run_target() {
  target=$1
  arch_flags=$2
  for run in 1 2 3; do
    build_release "$target" "$run" "$arch_flags"
    build_current "$target" "$run" "$arch_flags"
  done
}

run_target cortex-m0 '-mcpu=cortex-m0 -mthumb'
run_target cortex-m3 '-mcpu=cortex-m3 -mthumb'
run_target cortex-m4f '-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard'
run_target cortex-m7 '-mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard'

awk -F, '
  NR == 1 { next }
  {
    key=$1 FS $2
    signature=$4 FS $5 FS $6 FS $7 FS $8 FS $9 FS $10
    count[key]++
    if (!(key in first)) first[key]=signature
    if (first[key] != signature) {
      print "NHT_FLASH_OPT non-repeatable build: " key > "/dev/stderr"; bad=1
    }
    flash[key]=$8; ram[key]=$9
  }
  END {
    split("cortex-m0 cortex-m3 cortex-m4f cortex-m7", targets, " ")
    for (key in count) if (count[key] != 3) {
      print "NHT_FLASH_OPT missing repetitions: " key > "/dev/stderr"; bad=1
    }
    for (i in targets) {
      target=targets[i]; current="current," target; release="release," target
      if (flash[current] > flash[release] + 1024 || ram[current] > ram[release]) {
        print "NHT_FLASH_OPT " target " exceeds release budget" > "/dev/stderr"; bad=1
      }
    }
    exit bad
  }
' "$OUTPUT_DIR/measurements.csv"

awk -F, '
  NR == 1 || $3 != 1 { next }
  $1 == "current" { current_flash[$2]=$8; current_ram[$2]=$9 }
  $1 == "release" { release_flash[$2]=$8; release_ram[$2]=$9 }
  END {
    for (target in current_flash) {
      print target ": current Flash=" current_flash[target] " release Flash=" release_flash[target] \
        " delta=" current_flash[target]-release_flash[target] \
        "; current RAM=" current_ram[target] " release RAM=" release_ram[target]
    }
  }
' "$OUTPUT_DIR/measurements.csv" | sort | tee "$OUTPUT_DIR/comparison.txt"

printf 'NHT_FLASH_OPT release comparison PASS: baseline=%s@%s\n' \
  "$BASELINE_REF" "$BASELINE_COMMIT"
