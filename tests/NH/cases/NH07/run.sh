#!/bin/sh
set -eu

HOST_CC=${HOST_CC:-cc}
TOOLCHAIN_PREFIX=${TOOLCHAIN_PREFIX:-arm-none-eabi-}
ARM_CC=${TOOLCHAIN_PREFIX}gcc
ARM_NM=${TOOLCHAIN_PREFIX}nm
CUBE_CMAKE=${CUBE_CMAKE:-cube-cmake}
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/../../../.." && pwd)
FIXTURE_DIR="$SCRIPT_DIR"
if [ -n "${NH_OUTPUT_DIR:-}" ]; then
  BUILD_ROOT=$NH_OUTPUT_DIR
  mkdir -p "$BUILD_ROOT"
  KEEP_BUILD=1
else
  BUILD_ROOT=$(mktemp -d "${TMPDIR:-/tmp}/rtt-nh07-test.XXXXXX")
  KEEP_BUILD=0
fi

cleanup() {
  if [ "$KEEP_BUILD" -eq 0 ]; then
    rm -rf "$BUILD_ROOT"
  fi
}
trap cleanup EXIT INT TERM

expect_args() {
  profile=$1
  case "$profile" in
    default) printf '%s\n' '0 3 3 1024 16 64 0 168' ;;
    custom_a) printf '%s\n' '1 1 1 257 3 17 0 72' ;;
    custom_b) printf '%s\n' '1 2 2 129 5 9 1 120' ;;
    minimum) printf '%s\n' '1 1 1 8 2 1 0 72' ;;
    *) printf 'unknown NH07 profile: %s\n' "$profile" >&2; return 1 ;;
  esac
}

host_check() {
  profile=$1
  config_dir=$2
  set -- $(expect_args "$profile")
  "$HOST_CC" -std=c11 -Wall -Wextra -Werror -pedantic \
    -DEXPECT_USER_CFG="$1" -DEXPECT_UP="$2" -DEXPECT_DOWN="$3" \
    -DEXPECT_UP_SIZE="$4" -DEXPECT_DOWN_SIZE="$5" \
    -DEXPECT_PRINTF_SIZE="$6" -DEXPECT_LOG_INDEX="$7" \
    -DEXPECT_CB_SIZE="$8" \
    -I"$config_dir" -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
    -c "$FIXTURE_DIR/config_probe.c" \
    -o "$BUILD_ROOT/host_$profile.o"
  printf 'NH07 host config PASS: %s\n' "$profile"
}

make_build() {
  app=$1
  profile=$2
  config_dir=$3
  set -- $(expect_args "$profile")
  make -C "$app" -f Makefile \
    PREFIX="$TOOLCHAIN_PREFIX" RTT_CONFIG_DIR="$config_dir" \
    EXPECT_USER_CFG="$1" EXPECT_UP="$2" EXPECT_DOWN="$3" \
    EXPECT_UP_SIZE="$4" EXPECT_DOWN_SIZE="$5" \
    EXPECT_PRINTF_SIZE="$6" EXPECT_LOG_INDEX="$7" \
    EXPECT_CB_SIZE="$8" all

  object_count=$(find "$app/build" -name '*.o' -type f | wc -l | tr -d ' ')
  dependency_count=$(grep -l 'rtt_cfg.h' "$app"/build/*.d | wc -l | tr -d ' ')
  if [ "$object_count" -ne 6 ] || [ "$dependency_count" -lt 5 ]; then
    printf 'NH07 Make %s incomplete objects/dependencies: %s/%s\n' \
      "$profile" "$object_count" "$dependency_count" >&2
    return 1
  fi
  cb_size_hex=$("$ARM_NM" -S "$app/build/SEGGER_RTT.o" | \
    awk '$4 == "_SEGGER_RTT" { print $2 }')
  if [ "$((0x$cb_size_hex))" -ne "$8" ]; then
    printf 'NH07 Make %s control block size differs: %s\n' \
      "$profile" "$cb_size_hex" >&2
    return 1
  fi
  printf 'NH07 Make PASS: %s objects=%s cfg-deps=%s cb=%s\n' \
    "$profile" "$object_count" "$dependency_count" "$8"
}

cmake_build() {
  app=$1
  build=$2
  profile=$3
  config_dir=$4
  set -- $(expect_args "$profile")

  config_arg=
  if [ -n "$config_dir" ]; then
    config_dir=$(CDPATH= cd -- "$config_dir" && pwd -P)
    config_arg="-DARM_SEGGER_RTT_CONFIG_DIR=$config_dir"
  fi

  "$CUBE_CMAKE" \
    -S "$app" -B "$build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_SYSTEM_NAME=Generic -DCMAKE_SYSTEM_PROCESSOR=arm \
    -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
    -DCMAKE_C_COMPILER="$ARM_CC" -DCMAKE_ASM_COMPILER="$ARM_CC" \
    -DCMAKE_C_FLAGS="-mcpu=cortex-m0 -mthumb -ffunction-sections -fdata-sections" \
    -DCMAKE_ASM_FLAGS="-mcpu=cortex-m0 -mthumb" \
    -DARM_SEGGER_RTT_SOURCE_DIR="$PROJECT_DIR" \
    $config_arg \
    -DEXPECT_USER_CFG="$1" -DEXPECT_UP="$2" -DEXPECT_DOWN="$3" \
    -DEXPECT_UP_SIZE="$4" -DEXPECT_DOWN_SIZE="$5" \
    -DEXPECT_PRINTF_SIZE="$6" -DEXPECT_LOG_INDEX="$7" \
    -DEXPECT_CB_SIZE="$8"
  "$CUBE_CMAKE" --build "$build" --target all --

  library_object_count=$(find \
    "$build/arm_segger_rtt/CMakeFiles/arm_segger_rtt.dir" \
    -type f \( -name '*.o' -o -name '*.obj' \) | wc -l | tr -d ' ')
  probe_object_count=$(find \
    "$build/CMakeFiles/nh07_config_probe.dir" \
    -type f \( -name '*.o' -o -name '*.obj' \) | wc -l | tr -d ' ')
  object_count=$((library_object_count + probe_object_count))
  dependency_count=$(ninja -C "$build" -t deps | grep -c 'rtt_cfg.h')
  if [ "$object_count" -ne 6 ] || [ "$dependency_count" -lt 5 ]; then
    printf 'NH07 CMake %s incomplete objects/dependencies: %s/%s\n' \
      "$profile" "$object_count" "$dependency_count" >&2
    return 1
  fi
  if [ -n "$config_dir" ]; then
    if ! grep -F -- "-I$config_dir" "$build/build.ninja" >/dev/null; then
      printf 'NH07 CMake %s did not prioritize custom config path\n' \
        "$profile" >&2
      return 1
    fi
  fi
  segger_object=$(find "$build" -type f \
    \( -name 'SEGGER_RTT.c.o' -o -name 'SEGGER_RTT.c.obj' \) | head -1)
  cb_size_hex=$("$ARM_NM" -S "$segger_object" | \
    awk '$4 == "_SEGGER_RTT" { print $2 }')
  if [ "$((0x$cb_size_hex))" -ne "$8" ]; then
    printf 'NH07 CMake %s control block size differs: %s\n' \
      "$profile" "$cb_size_hex" >&2
    return 1
  fi
  printf 'NH07 CMake PASS: %s objects=%s cfg-deps=%s cb=%s\n' \
    "$profile" "$object_count" "$dependency_count" "$8"
}

expect_compile_failure() {
  name=$1
  expected=$2
  config_dir=$3
  source=$4
  log="$BUILD_ROOT/$name.log"
  if "$HOST_CC" -std=c11 -Wall -Wextra -Werror -pedantic \
      -I"$config_dir" -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
      -fsyntax-only "$source" >"$log" 2>&1; then
    printf 'NH07 %s compiled unexpectedly\n' "$name" >&2
    return 1
  fi
  if ! grep -Fq "$expected" "$log"; then
    printf 'NH07 %s failed for an unexpected reason\n' "$name" >&2
    sed -n '1,80p' "$log" >&2
    return 1
  fi
  printf 'NH07 invalid config PASS: %s\n' "$name"
}

host_check default "$FIXTURE_DIR/configs/default"
host_check custom_a "$FIXTURE_DIR/configs/custom_a"
host_check custom_b "$FIXTURE_DIR/configs/custom_b"
host_check minimum "$FIXTURE_DIR/configs/minimum"

"$HOST_CC" -std=c11 -Wall -Wextra -Werror -pedantic \
  -I"$FIXTURE_DIR/configs/minimum" -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
  "$PROJECT_DIR/rtt_printf.c" "$FIXTURE_DIR/printf_minimum_test.c" \
  -o "$BUILD_ROOT/printf_minimum"
"$BUILD_ROOT/printf_minimum"

expect_compile_failure printf_zero \
  'SEGGER_RTT_PRINTF_BUFFER_SIZE must be at least 1' \
  "$FIXTURE_DIR/configs/invalid_zero" "$PROJECT_DIR/rtt_printf.c"
expect_compile_failure channel_out_of_range \
  'RTT_LOG_BUFFER_INDEX must be less than SEGGER_RTT_MAX_NUM_UP_BUFFERS' \
  "$FIXTURE_DIR/configs/invalid_channel" "$PROJECT_DIR/rtt_log.c"

make_app="$BUILD_ROOT/make_app"
mkdir -p "$make_app/config"
cp "$FIXTURE_DIR/Makefile" "$FIXTURE_DIR/config_probe.c" "$make_app/"
mkdir -p "$make_app/ARM_SEGGER_RTT"
cp "$PROJECT_DIR/segger_rtt.mk" "$PROJECT_DIR"/rtt_*.c \
  "$PROJECT_DIR"/rtt_*.h "$PROJECT_DIR/rtt_cfg.h" \
  "$make_app/ARM_SEGGER_RTT/"
cp -R "$PROJECT_DIR/RTT" "$make_app/ARM_SEGGER_RTT/RTT"

cp "$FIXTURE_DIR/configs/default/rtt_cfg.h" "$make_app/rtt_cfg.h"
make_build "$make_app" default .
make -C "$make_app" -f Makefile clean

cp "$FIXTURE_DIR/configs/custom_a/rtt_cfg.h" "$make_app/config/rtt_cfg.h"
make_build "$make_app" custom_a config
make_a_segger_hash=$(shasum -a 256 "$make_app/build/SEGGER_RTT.o" | awk '{print $1}')
make_a_probe_hash=$(shasum -a 256 "$make_app/build/config_probe.o" | awk '{print $1}')
make -C "$make_app" -f Makefile clean
test ! -d "$make_app/build"
cp "$FIXTURE_DIR/configs/custom_b/rtt_cfg.h" "$make_app/config/rtt_cfg.h"
make_build "$make_app" custom_b config
make_b_segger_hash=$(shasum -a 256 "$make_app/build/SEGGER_RTT.o" | awk '{print $1}')
make_b_probe_hash=$(shasum -a 256 "$make_app/build/config_probe.o" | awk '{print $1}')
if [ "$make_a_segger_hash" = "$make_b_segger_hash" ] || \
   [ "$make_a_probe_hash" = "$make_b_probe_hash" ]; then
  printf '%s\n' 'NH07 Make clean rebuild retained stale objects' >&2
  exit 1
fi
printf '%s\n' 'NH07 Make clean rebuild PASS: custom_a -> custom_b'

cmake_app="$BUILD_ROOT/cmake_app"
mkdir -p "$cmake_app/config"
cp "$FIXTURE_DIR/CMakeLists.txt" "$FIXTURE_DIR/config_probe.c" "$cmake_app/"
cp "$FIXTURE_DIR/configs/default/rtt_cfg.h" "$cmake_app/rtt_cfg.h"
cmake_build "$cmake_app" "$BUILD_ROOT/cmake_default" default ""

cp "$FIXTURE_DIR/configs/custom_a/rtt_cfg.h" "$cmake_app/config/rtt_cfg.h"
cmake_build "$cmake_app" "$BUILD_ROOT/cmake_custom" custom_a "$cmake_app/config"
cmake_a_segger=$(find "$BUILD_ROOT/cmake_custom" -type f \
  \( -name 'SEGGER_RTT.c.o' -o -name 'SEGGER_RTT.c.obj' \) | head -1)
cmake_a_probe=$(find "$BUILD_ROOT/cmake_custom" -type f \
  \( -name 'config_probe.c.o' -o -name 'config_probe.c.obj' \) | head -1)
cmake_a_segger_hash=$(shasum -a 256 "$cmake_a_segger" | awk '{print $1}')
cmake_a_probe_hash=$(shasum -a 256 "$cmake_a_probe" | awk '{print $1}')
rm -rf "$BUILD_ROOT/cmake_custom"
cp "$FIXTURE_DIR/configs/custom_b/rtt_cfg.h" "$cmake_app/config/rtt_cfg.h"
cmake_build "$cmake_app" "$BUILD_ROOT/cmake_custom" custom_b "$cmake_app/config"
cmake_b_segger=$(find "$BUILD_ROOT/cmake_custom" -type f \
  \( -name 'SEGGER_RTT.c.o' -o -name 'SEGGER_RTT.c.obj' \) | head -1)
cmake_b_probe=$(find "$BUILD_ROOT/cmake_custom" -type f \
  \( -name 'config_probe.c.o' -o -name 'config_probe.c.obj' \) | head -1)
cmake_b_segger_hash=$(shasum -a 256 "$cmake_b_segger" | awk '{print $1}')
cmake_b_probe_hash=$(shasum -a 256 "$cmake_b_probe" | awk '{print $1}')
if [ "$cmake_a_segger_hash" = "$cmake_b_segger_hash" ] || \
   [ "$cmake_a_probe_hash" = "$cmake_b_probe_hash" ]; then
  printf '%s\n' 'NH07 CMake clean rebuild retained stale objects' >&2
  exit 1
fi
printf '%s\n' 'NH07 CMake clean rebuild PASS: custom_a -> custom_b'

printf '%s\n' 'NH07 PASS: host, Make, cube-cmake, dependencies, layout, clean rebuild, invalid configs'
