#!/bin/sh
set -eu

TOOLCHAIN_PREFIX=${TOOLCHAIN_PREFIX:-arm-none-eabi-}
ARM_NM=${TOOLCHAIN_PREFIX}nm
ARM_SIZE=${TOOLCHAIN_PREFIX}size
JOBS=${NH_JOBS:-8}
CUBE_CMAKE=${CUBE_CMAKE:-cube-cmake}
CUBE=${CUBE:-cube}
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/../../../.." && pwd)
WORKSPACE=${NH_WORKSPACE:-$(CDPATH= cd -- "$PROJECT_DIR/.." && pwd)}
EVIDENCE_DIR=${NH09_EVIDENCE_DIR:-$PROJECT_DIR/TEST_EVIDENCE/TEST_EVIDENCE_NH09_20260821}
TEMP_ROOT=$(mktemp -d "${TMPDIR:-/tmp}/rtt-nh09-test.XXXXXX")
CUBE_GCC=${CUBE_GCC:-${TOOLCHAIN_PREFIX}gcc}
FEATURE_CONFIG_DIR="$SCRIPT_DIR/configs/features"
NH_LINK_LIBS='-lc -lm -lnosys -Wl,--undefined=SEGGER_RTT_WriteNoLock'
NH_CMAKE_LINK_OVERLAY="$SCRIPT_DIR/../../support/link_anchor.cmake"

cleanup() {
  rm -rf "$TEMP_ROOT"
}
trap cleanup EXIT INT TERM

mkdir -p "$EVIDENCE_DIR/logs" "$EVIDENCE_DIR/hashes" \
  "$EVIDENCE_DIR/metadata" "$EVIDENCE_DIR/additional"
FAILURE_FILE="$EVIDENCE_DIR/failures.txt"
: >"$FAILURE_FILE"
: >"$EVIDENCE_DIR/summary.csv"
printf '%s\n' 'project,type,configuration,text,data,bss,expected_asm_path,result' \
  >"$EVIDENCE_DIR/summary.csv"

fail() {
  printf 'NH09 FAIL: %s\n' "$1" >&2
  exit 1
}

record_failure() {
  printf '%s\n' "$1" >>"$FAILURE_FILE"
  printf 'NH09 REQUIREMENT FAILURE: %s\n' "$1" >&2
}

require_file() {
  test -f "$1" || fail "missing artifact $1"
}

record_size() {
  name=$1
  type=$2
  config=$3
  elf=$4
  asm_path=$5
  set -- $("$ARM_SIZE" "$elf" | awk 'NR == 2 { print $1, $2, $3 }')
  printf '%s,%s,%s,%s,%s,%s,%s,PASS\n' \
    "$name" "$type" "$config" "$1" "$2" "$3" "$asm_path" \
    >>"$EVIDENCE_DIR/summary.csv"
}

check_asm_symbol() {
  elf=$1
  expected=$2
  if [ "$expected" = ASM ]; then
    "$ARM_NM" "$elf" | grep -q 'SEGGER_RTT_ASM_WriteSkipNoLock' || \
      record_failure "ASM RTT symbol missing from final ELF: $elf"
  else
    if "$ARM_NM" "$elf" | grep -q 'SEGGER_RTT_ASM_WriteSkipNoLock'; then
      record_failure "unexpected ASM RTT symbol in final ELF: $elf"
    fi
  fi
}

write_hashes() {
  output=$1
  shift
  shasum -a 256 "$@" | awk '{ print $1, $2 }' >"$output"
}

check_make_build() {
  name=$1
  project=$2
  target=$3
  config=$4
  cpu=$5
  extra_arch=$6
  asm_path=$7
  log=$8
  build="$project/build"
  elf="$build/$target.elf"
  map="$build/$target.map"

  require_file "$elf"
  require_file "$build/$target.hex"
  require_file "$build/$target.bin"
  require_file "$map"
  for object in SEGGER_RTT.o rtt_printf.o rtt_log.o rtt_float.o SEGGER_RTT_ASM_ARMv7M.o; do
    require_file "$build/$object"
  done
  if [ "$asm_path" = ASM ]; then
    "$ARM_NM" "$build/SEGGER_RTT_ASM_ARMv7M.o" | \
      grep -q 'SEGGER_RTT_ASM_WriteSkipNoLock' || \
      fail "$name Make $config ASM object missing implementation"
  fi
  grep -F -- "-mcpu=$cpu" "$log" >/dev/null || fail "$name Make $config CPU flags"
  if [ -n "$extra_arch" ]; then
    grep -F -- "$extra_arch" "$log" >/dev/null || fail "$name Make $config FPU flags"
  fi
  for source in SEGGER_RTT.c rtt_printf.c rtt_log.c rtt_float.c SEGGER_RTT_ASM_ARMv7M.S; do
    grep -F "$source" "$log" >/dev/null || fail "$name Make $config missing $source"
  done
  grep -F ' -I. -IARM_SEGGER_RTT' "$log" >/dev/null || \
    fail "$name Make $config root config path"
  grep -F 'rtt_printf.c' "$log" | grep -F -- '-Os' >/dev/null || \
    fail "$name Make $config RTT optimization"
  check_asm_symbol "$elf" "$asm_path"
  "$ARM_NM" "$elf" | grep -E 'modff|__aeabi_.*div' \
    >"$EVIDENCE_DIR/metadata/${name}_make_${config}_runtime_symbols.txt" || true
  record_size "$name" Make "$config" "$elf" "$asm_path"
}

run_make_project() {
  name=$1
  project=$2
  target=$3
  cpu=$4
  extra_arch=$5
  asm_path=$6
  make -C "$project" PREFIX="$TOOLCHAIN_PREFIX" clean \
    >"$EVIDENCE_DIR/logs/${name}_make_clean_initial.log" 2>&1
  make -C "$project" PREFIX="$TOOLCHAIN_PREFIX" -j"$JOBS" DEBUG=1 OPT=-Og \
    >"$EVIDENCE_DIR/logs/${name}_make_debug.log" 2>&1 || \
    fail "$name Make Debug build"
  check_make_build "$name" "$project" "$target" Debug "$cpu" \
    "$extra_arch" "$asm_path" "$EVIDENCE_DIR/logs/${name}_make_debug.log"

  make -C "$project" PREFIX="$TOOLCHAIN_PREFIX" -j"$JOBS" DEBUG=1 OPT=-Og \
    >"$EVIDENCE_DIR/logs/${name}_make_incremental.log" 2>&1 || \
    fail "$name Make incremental build"
  if grep -E 'arm-none-eabi-gcc .* -c ' \
      "$EVIDENCE_DIR/logs/${name}_make_incremental.log" >/dev/null; then
    fail "$name Make incremental build recompiled objects"
  fi

  make -C "$project" PREFIX="$TOOLCHAIN_PREFIX" clean \
    >"$EVIDENCE_DIR/logs/${name}_make_clean_release.log" 2>&1
  make -C "$project" PREFIX="$TOOLCHAIN_PREFIX" -j"$JOBS" DEBUG=0 OPT=-Os \
    >"$EVIDENCE_DIR/logs/${name}_make_release_first.log" 2>&1 || \
    fail "$name Make Release build"
  check_make_build "$name" "$project" "$target" Release "$cpu" \
    "$extra_arch" "$asm_path" "$EVIDENCE_DIR/logs/${name}_make_release_first.log"
  write_hashes "$EVIDENCE_DIR/hashes/${name}_make_release_first.txt" \
    "$project/build/$target.elf" "$project/build/$target.hex" \
    "$project/build/$target.bin" "$project/build/$target.map"

  make -C "$project" PREFIX="$TOOLCHAIN_PREFIX" clean \
    >"$EVIDENCE_DIR/logs/${name}_make_clean_rebuild.log" 2>&1
  make -C "$project" PREFIX="$TOOLCHAIN_PREFIX" -j"$JOBS" DEBUG=0 OPT=-Os \
    >"$EVIDENCE_DIR/logs/${name}_make_release_rebuild.log" 2>&1 || \
    fail "$name Make clean rebuild"
  write_hashes "$EVIDENCE_DIR/hashes/${name}_make_release_rebuild.txt" \
    "$project/build/$target.elf" "$project/build/$target.hex" \
    "$project/build/$target.bin" "$project/build/$target.map"
  cmp "$EVIDENCE_DIR/hashes/${name}_make_release_first.txt" \
      "$EVIDENCE_DIR/hashes/${name}_make_release_rebuild.txt" >/dev/null || \
    fail "$name Make clean rebuild differs"
  printf 'NH09 Make PASS: %s Debug/Release incremental clean-rebuild %s\n' \
    "$name" "$asm_path"
}

find_cmake_artifact() {
  build=$1
  target=$2
  suffix=$3
  find "$build" -maxdepth 2 -type f -name "$target$suffix" | head -1
}

check_cmake_build() {
  name=$1
  project=$2
  target=$3
  config=$4
  cpu=$5
  extra_arch=$6
  asm_path=$7
  build="$project/build/$config"
  commands="$build/compile_commands.json"
  elf=$(find_cmake_artifact "$build" "$target" '.elf')
  hex=$(find_cmake_artifact "$build" "$target" '.hex')
  bin=$(find_cmake_artifact "$build" "$target" '.bin')
  map=$(find_cmake_artifact "$build" "$target" '.map')

  require_file "$commands"
  require_file "$elf"
  require_file "$hex"
  require_file "$bin"
  require_file "$map"
  grep -F -- "-mcpu=$cpu" "$commands" >/dev/null || fail "$name CMake $config CPU flags"
  if [ -n "$extra_arch" ]; then
    grep -F -- "$extra_arch" "$commands" >/dev/null || fail "$name CMake $config FPU flags"
  fi
  for source in SEGGER_RTT.c rtt_printf.c rtt_log.c rtt_float.c SEGGER_RTT_ASM_ARMv7M.S; do
    grep -F "$source" "$commands" >/dev/null || fail "$name CMake $config missing $source"
  done
  grep -F -- "-I$project" "$commands" >/dev/null || \
    fail "$name CMake $config root config path"
  asm_object=$(find "$build/ARM_SEGGER_RTT" -type f \
    \( -name 'SEGGER_RTT_ASM_ARMv7M.S.o' -o \
       -name 'SEGGER_RTT_ASM_ARMv7M.S.obj' \) | head -1)
  require_file "$asm_object"
  if [ "$asm_path" = ASM ]; then
    "$ARM_NM" "$asm_object" | grep -q 'SEGGER_RTT_ASM_WriteSkipNoLock' || \
      fail "$name CMake $config ASM object missing implementation"
  fi
  grep -F 'rtt_printf.c' "$commands" | grep -F -- '-Os' >/dev/null || \
    fail "$name CMake $config RTT optimization"
  check_asm_symbol "$elf" "$asm_path"
  load_count=$(grep -c '^LOAD .*libarm_segger_rtt.a' "$map" || true)
  test "$load_count" -eq 1 || \
    fail "$name CMake $config RTT library load count $load_count"
  "$ARM_NM" "$elf" | grep -E 'modff|__aeabi_.*div' \
    >"$EVIDENCE_DIR/metadata/${name}_cmake_${config}_runtime_symbols.txt" || true
  record_size "$name" CMake "$config" "$elf" "$asm_path"
}

run_cmake_project() {
  name=$1
  project=$2
  target=$3
  cpu=$4
  extra_arch=$5
  asm_path=$6
  make -C "$project" info \
    >"$EVIDENCE_DIR/logs/${name}_cmake_info.log" 2>&1 || fail "$name CMake info"
  make -C "$project" clean \
    >"$EVIDENCE_DIR/logs/${name}_cmake_clean_initial.log" 2>&1
  make -C "$project" preset_debug \
    >"$EVIDENCE_DIR/logs/${name}_cmake_preset_debug.log" 2>&1 || \
    fail "$name CMake Debug configure"
  make -C "$project" d \
    >"$EVIDENCE_DIR/logs/${name}_cmake_debug.log" 2>&1 || \
    fail "$name CMake Debug build"
  check_cmake_build "$name" "$project" "$target" Debug "$cpu" \
    "$extra_arch" "$asm_path"

  "$CUBE_CMAKE" --build "$project/build/Debug" --target all -- \
    >"$EVIDENCE_DIR/logs/${name}_cmake_incremental.log" 2>&1 || \
    fail "$name CMake incremental build"
  grep -F 'no work to do' "$EVIDENCE_DIR/logs/${name}_cmake_incremental.log" \
    >/dev/null || fail "$name CMake incremental build did work"

  make -C "$project" preset_release \
    >"$EVIDENCE_DIR/logs/${name}_cmake_preset_release_first.log" 2>&1 || \
    fail "$name CMake Release configure"
  make -C "$project" r \
    >"$EVIDENCE_DIR/logs/${name}_cmake_release_first.log" 2>&1 || \
    fail "$name CMake Release build"
  check_cmake_build "$name" "$project" "$target" Release "$cpu" \
    "$extra_arch" "$asm_path"
  build="$project/build/Release"
  elf=$(find_cmake_artifact "$build" "$target" '.elf')
  hex=$(find_cmake_artifact "$build" "$target" '.hex')
  bin=$(find_cmake_artifact "$build" "$target" '.bin')
  map=$(find_cmake_artifact "$build" "$target" '.map')
  write_hashes "$EVIDENCE_DIR/hashes/${name}_cmake_release_first.txt" \
    "$elf" "$hex" "$bin" "$map"

  make -C "$project" preset_release \
    >"$EVIDENCE_DIR/logs/${name}_cmake_preset_release_rebuild.log" 2>&1 || \
    fail "$name CMake clean reconfigure"
  make -C "$project" r \
    >"$EVIDENCE_DIR/logs/${name}_cmake_release_rebuild.log" 2>&1 || \
    fail "$name CMake clean rebuild"
  build="$project/build/Release"
  elf=$(find_cmake_artifact "$build" "$target" '.elf')
  hex=$(find_cmake_artifact "$build" "$target" '.hex')
  bin=$(find_cmake_artifact "$build" "$target" '.bin')
  map=$(find_cmake_artifact "$build" "$target" '.map')
  write_hashes "$EVIDENCE_DIR/hashes/${name}_cmake_release_rebuild.txt" \
    "$elf" "$hex" "$bin" "$map"
  cmp "$EVIDENCE_DIR/hashes/${name}_cmake_release_first.txt" \
      "$EVIDENCE_DIR/hashes/${name}_cmake_release_rebuild.txt" >/dev/null || \
    fail "$name CMake clean rebuild differs"
  printf 'NH09 CMake PASS: %s Debug/Release incremental clean-rebuild %s\n' \
    "$name" "$asm_path"
}

run_make_fallback() {
  name=$1
  project=$2
  target=$3
  defs=$4
  build="$TEMP_ROOT/${name}_make_c_fallback"
  log="$EVIDENCE_DIR/additional/${name}_make_c_fallback.log"
  make -C "$project" PREFIX="$TOOLCHAIN_PREFIX" -j"$JOBS" \
    BUILD_DIR="$build" DEBUG=0 OPT=-Os \
    C_DEFS="$defs -DRTT_USE_ASM=0" >"$log" 2>&1 || \
    fail "$name Make C fallback build"
  require_file "$build/$target.elf"
  if "$ARM_NM" "$build/$target.elf" | grep -q 'SEGGER_RTT_ASM_WriteSkipNoLock'; then
    fail "$name Make C fallback retained ASM symbol"
  fi
  "$ARM_NM" "$build/SEGGER_RTT.o" | grep -q 'SEGGER_RTT_WriteSkipNoLock' || \
    fail "$name Make C fallback object missing C implementation"
  printf 'NH09 Make C fallback PASS: %s\n' "$name"
}

run_cmake_fallback() {
  name=$1
  project=$2
  target=$3
  build="$TEMP_ROOT/${name}_cmake_c_fallback"
  log="$EVIDENCE_DIR/additional/${name}_cmake_c_fallback.log"
  "$CUBE_CMAKE" -S "$project" -B "$build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$project/cmake/gcc-arm-none-eabi.cmake" \
    -DCMAKE_C_FLAGS=-DRTT_USE_ASM=0 \
    -DCMAKE_ASM_FLAGS=-DRTT_USE_ASM=0 >"$log" 2>&1 || \
    fail "$name CMake C fallback configure"
  "$CUBE_CMAKE" --build "$build" --target all -- \
    >>"$log" 2>&1 || fail "$name CMake C fallback build"
  elf=$(find_cmake_artifact "$build" "$target" '.elf')
  require_file "$elf"
  if "$ARM_NM" "$elf" | grep -q 'SEGGER_RTT_ASM_WriteSkipNoLock'; then
    fail "$name CMake C fallback retained ASM symbol"
  fi
  object=$(find "$build/ARM_SEGGER_RTT" -type f \
    \( -name 'SEGGER_RTT.c.o' -o -name 'SEGGER_RTT.c.obj' \) | head -1)
  require_file "$object"
  "$ARM_NM" "$object" | grep -q 'SEGGER_RTT_WriteSkipNoLock' || \
    fail "$name CMake C fallback object missing C implementation"
  printf 'NH09 CMake C fallback PASS: %s\n' "$name"
}

run_override_tests() {
  make_project="$WORKSPACE/stm32f042g6make"
  cmake_project="$WORKSPACE/stm32f042g6cmake"
  config_dir=$(CDPATH= cd -- "$SCRIPT_DIR/../NH07/configs/custom_b" && pwd -P)
  make_build="$TEMP_ROOT/f042_make_override"
  cmake_build="$TEMP_ROOT/f042_cmake_override"
  make_log="$EVIDENCE_DIR/additional/f042_make_config_and_opt_override.log"
  cmake_log="$EVIDENCE_DIR/additional/f042_cmake_config_and_opt_override.log"

  make -C "$make_project" PREFIX="$TOOLCHAIN_PREFIX" -j"$JOBS" \
    BUILD_DIR="$make_build" DEBUG=1 OPT=-O0 \
    RTT_CONFIG_DIR="$config_dir" ARM_SEGGER_RTT_OPTIMIZATION= \
    >"$make_log" 2>&1 || fail 'F042 Make config/optimization override'
  require_file "$make_build/stm32f042g6make.elf"
  grep -F -- "-I$config_dir" "$make_log" >/dev/null || fail 'F042 Make config path'
  rtt_line=$(grep 'rtt_printf.c' "$make_log" | head -1)
  printf '%s\n' "$rtt_line" | grep -F -- '-O0' >/dev/null || \
    fail 'F042 Make inherited optimization missing'
  if printf '%s\n' "$rtt_line" | grep -F -- '-Os' >/dev/null; then
    fail 'F042 Make empty RTT optimization still used -Os'
  fi

  "$CUBE_CMAKE" -S "$cmake_project" -B "$cmake_build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_TOOLCHAIN_FILE="$cmake_project/cmake/gcc-arm-none-eabi.cmake" \
    -DARM_SEGGER_RTT_CONFIG_DIR="$config_dir" \
    -DARM_SEGGER_RTT_OPTIMIZATION= >"$cmake_log" 2>&1 || \
    fail 'F042 CMake config/optimization override configure'
  "$CUBE_CMAKE" --build "$cmake_build" --target all -- \
    >>"$cmake_log" 2>&1 || fail 'F042 CMake config/optimization override build'
  require_file "$cmake_build/compile_commands.json"
  grep -F "$config_dir" "$cmake_build/compile_commands.json" >/dev/null || \
    fail 'F042 CMake config path'
  rtt_line=$(grep 'rtt_printf.c' "$cmake_build/compile_commands.json" | head -1)
  printf '%s\n' "$rtt_line" | grep -F -- '-O0' >/dev/null || \
    fail 'F042 CMake inherited optimization missing'
  if printf '%s\n' "$rtt_line" | grep -F -- '-Os' >/dev/null; then
    fail 'F042 CMake empty RTT optimization still used -Os'
  fi
  printf '%s\n' 'NH09 config path and empty optimization override PASS: Make/CMake'
}

check_feature_objects() {
  name=$1
  type=$2
  log=$3
  rtt_log_object=$4
  rtt_float_object=$5
  rtt_c_object=$6
  asm_object=$7
  expected_path=$8

  for source in SEGGER_RTT.c rtt_printf.c rtt_log.c rtt_float.c SEGGER_RTT_ASM_ARMv7M.S; do
    grep -F "$source" "$log" | grep -F -- "-I$FEATURE_CONFIG_DIR" >/dev/null || \
      fail "$name $type feature config missing from $source"
  done
  "$ARM_NM" "$rtt_log_object" | grep -q ' RTT_LogI32$' || \
    fail "$name $type typed integer object symbol"
  "$ARM_NM" "$rtt_float_object" | grep -q ' RTT_LogF32$' || \
    fail "$name $type typed float object symbol"
  if [ "$expected_path" = ASM ]; then
    "$ARM_NM" -u "$rtt_c_object" | grep -q 'SEGGER_RTT_ASM_WriteSkipNoLock' || \
      fail "$name $type feature C object missing ASM reference"
    "$ARM_NM" "$asm_object" | grep -q 'SEGGER_RTT_ASM_WriteSkipNoLock' || \
      fail "$name $type feature ASM object missing implementation"
  else
    if "$ARM_NM" -u "$rtt_c_object" | grep -q 'SEGGER_RTT_ASM_WriteSkipNoLock'; then
      fail "$name $type M0 feature C object referenced ASM"
    fi
  fi
  {
    "$ARM_NM" "$rtt_log_object" | grep 'RTT_LogI32'
    "$ARM_NM" "$rtt_float_object" | grep 'RTT_LogF32'
    "$ARM_NM" -u "$rtt_c_object" | grep 'SEGGER_RTT_ASM_WriteSkipNoLock' || true
    "$ARM_NM" "$asm_object" | grep 'SEGGER_RTT_ASM_WriteSkipNoLock' || true
  } >"$EVIDENCE_DIR/metadata/${name}_${type}_feature_symbols.txt"
}

run_make_features() {
  name=$1
  project=$2
  target=$3
  expected_path=$4
  build="$TEMP_ROOT/${name}_make_features"
  log="$EVIDENCE_DIR/additional/${name}_make_features.log"

  make -C "$project" PREFIX="$TOOLCHAIN_PREFIX" -j"$JOBS" \
    BUILD_DIR="$build" DEBUG=0 OPT=-Os \
    LIBS="$NH_LINK_LIBS" \
    RTT_CONFIG_DIR="$FEATURE_CONFIG_DIR" >"$log" 2>&1 || \
    fail "$name Make feature build"
  require_file "$build/$target.elf"
  check_feature_objects "$name" make "$log" "$build/rtt_log.o" \
    "$build/rtt_float.o" "$build/SEGGER_RTT.o" \
    "$build/SEGGER_RTT_ASM_ARMv7M.o" "$expected_path"
  check_asm_symbol "$build/$target.elf" "$expected_path"
  printf 'NH09 Make feature propagation PASS: %s typed typed-float float-fast skip-%s\n' \
    "$name" "$expected_path"
}

run_cmake_features() {
  name=$1
  project=$2
  target=$3
  expected_path=$4
  build="$TEMP_ROOT/${name}_cmake_features"
  log="$EVIDENCE_DIR/additional/${name}_cmake_features.log"

  "$CUBE_CMAKE" -S "$project" -B "$build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$project/cmake/gcc-arm-none-eabi.cmake" \
    -DCMAKE_PROJECT_INCLUDE="$NH_CMAKE_LINK_OVERLAY" \
    -DARM_SEGGER_RTT_CONFIG_DIR="$FEATURE_CONFIG_DIR" >"$log" 2>&1 || \
    fail "$name CMake feature configure"
  "$CUBE_CMAKE" --build "$build" --target all -- \
    >>"$log" 2>&1 || fail "$name CMake feature build"
  elf=$(find_cmake_artifact "$build" "$target" '.elf')
  require_file "$elf"
  rtt_log_object=$(find "$build/ARM_SEGGER_RTT" -type f \
    \( -name 'rtt_log.c.o' -o -name 'rtt_log.c.obj' \) | head -1)
  rtt_float_object=$(find "$build/ARM_SEGGER_RTT" -type f \
    \( -name 'rtt_float.c.o' -o -name 'rtt_float.c.obj' \) | head -1)
  rtt_c_object=$(find "$build/ARM_SEGGER_RTT" -type f \
    \( -name 'SEGGER_RTT.c.o' -o -name 'SEGGER_RTT.c.obj' \) | head -1)
  asm_object=$(find "$build/ARM_SEGGER_RTT" -type f \
    \( -name 'SEGGER_RTT_ASM_ARMv7M.S.o' -o \
       -name 'SEGGER_RTT_ASM_ARMv7M.S.obj' \) | head -1)
  require_file "$rtt_log_object"
  require_file "$rtt_float_object"
  require_file "$rtt_c_object"
  require_file "$asm_object"
  check_feature_objects "$name" cmake "$build/compile_commands.json" \
    "$rtt_log_object" "$rtt_float_object" "$rtt_c_object" \
    "$asm_object" "$expected_path"
  check_asm_symbol "$elf" "$expected_path"
  printf 'NH09 CMake feature propagation PASS: %s typed typed-float float-fast skip-%s\n' \
    "$name" "$expected_path"
}

{
  date '+%Y-%m-%d %H:%M:%S %Z (%z)'
  uname -a
  if command -v sw_vers >/dev/null 2>&1; then sw_vers; fi
  make --version | head -1
  "${TOOLCHAIN_PREFIX}gcc" --version | head -1
  "$CUBE_GCC" --version | head -1
  "$CUBE" --version
  "$CUBE_CMAKE" --version
  ninja --version
} >"$EVIDENCE_DIR/metadata/toolchain.txt" 2>&1

for project in \
  stm32f042g6make stm32f042g6cmake \
  stm32f103c8make stm32f103c8cmake \
  stm32f411cemake stm32f411cecmake \
  stm32h7b0vbmake stm32h7b0vbcmake; do
  path="$WORKSPACE/$project"
  {
    printf 'project=%s\n' "$path"
    git -C "$path" rev-parse HEAD
    git -C "$path" status --short --untracked-files=no
    git -C "$path/ARM_SEGGER_RTT" rev-parse HEAD
  } >"$EVIDENCE_DIR/metadata/${project}_version.txt"
done

run_make_project f042 "$WORKSPACE/stm32f042g6make" stm32f042g6make cortex-m0 '' C
run_cmake_project f042 "$WORKSPACE/stm32f042g6cmake" stm32f042g6cmake cortex-m0 '' C

run_make_project f103 "$WORKSPACE/stm32f103c8make" stm32f103c8make cortex-m3 '' C
run_make_project f411 "$WORKSPACE/stm32f411cemake" stm32f411cemake cortex-m4 \
  '-mfpu=fpv4-sp-d16 -mfloat-abi=hard' C
run_make_project h7b0 "$WORKSPACE/stm32h7b0vbmake" stm32h7b0vbmake cortex-m7 \
  '-mfpu=fpv5-d16 -mfloat-abi=hard' C

run_cmake_project f103 "$WORKSPACE/stm32f103c8cmake" stm32f103c8cmake cortex-m3 '' C
run_cmake_project f411 "$WORKSPACE/stm32f411cecmake" stm32f411cecmake cortex-m4 \
  '-mfpu=fpv4-sp-d16 -mfloat-abi=hard' C
run_cmake_project h7b0 "$WORKSPACE/stm32h7b0vbcmake" stm32h7b0vbcmake cortex-m7 \
  '-mfpu=fpv5-d16 -mfloat-abi=hard' C

run_override_tests

run_make_features f042 "$WORKSPACE/stm32f042g6make" stm32f042g6make C
run_make_features f103 "$WORKSPACE/stm32f103c8make" stm32f103c8make ASM
run_make_features f411 "$WORKSPACE/stm32f411cemake" stm32f411cemake ASM
run_make_features h7b0 "$WORKSPACE/stm32h7b0vbmake" stm32h7b0vbmake ASM
run_cmake_features f042 "$WORKSPACE/stm32f042g6cmake" stm32f042g6cmake C
run_cmake_features f103 "$WORKSPACE/stm32f103c8cmake" stm32f103c8cmake ASM
run_cmake_features f411 "$WORKSPACE/stm32f411cecmake" stm32f411cecmake ASM
run_cmake_features h7b0 "$WORKSPACE/stm32h7b0vbcmake" stm32h7b0vbcmake ASM

run_make_fallback f103 "$WORKSPACE/stm32f103c8make" stm32f103c8make \
  '-DUSE_HAL_DRIVER -DSTM32F103xB'
run_make_fallback f411 "$WORKSPACE/stm32f411cemake" stm32f411cemake \
  '-DUSE_HAL_DRIVER -DSTM32F411xE'
run_make_fallback h7b0 "$WORKSPACE/stm32h7b0vbmake" stm32h7b0vbmake \
  '-DUSE_HAL_DRIVER -DSTM32H7B0xx'
run_cmake_fallback f103 "$WORKSPACE/stm32f103c8cmake" stm32f103c8cmake
run_cmake_fallback f411 "$WORKSPACE/stm32f411cecmake" stm32f411cecmake
run_cmake_fallback h7b0 "$WORKSPACE/stm32h7b0vbcmake" stm32h7b0vbcmake

if test -s "$FAILURE_FILE"; then
  printf '%s\n' 'NH09 completed with requirement failures:' >&2
  sed -n '1,120p' "$FAILURE_FILE" >&2
  exit 1
fi
printf '%s\n' 'NH09 PASS: 8 projects, Debug/Release, artifacts, flags, ASM/C, config, optimization, incremental, reproducibility'
