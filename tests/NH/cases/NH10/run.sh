#!/bin/sh
set -eu

LC_ALL=C
export LC_ALL

TOOLCHAIN_PREFIX=${TOOLCHAIN_PREFIX:-arm-none-eabi-}
ARM_SIZE=${TOOLCHAIN_PREFIX}size
ARM_NM=${TOOLCHAIN_PREFIX}nm
ARM_OBJDUMP=${TOOLCHAIN_PREFIX}objdump
JOBS=${NH_JOBS:-8}
CUBE_CMAKE=${CUBE_CMAKE:-cube-cmake}
CUBE=${CUBE:-cube}
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/../../../.." && pwd)
WORKSPACE=${NH_WORKSPACE:-$(CDPATH= cd -- "$PROJECT_DIR/.." && pwd)}
EVIDENCE_DIR=${NH10_EVIDENCE_DIR:-$PROJECT_DIR/TEST_EVIDENCE/TEST_EVIDENCE_NH10_20260821}
TEMP_ROOT=$(mktemp -d "${TMPDIR:-/tmp}/rtt-nh10-test.XXXXXX")
CUBE_GCC=${CUBE_GCC:-${TOOLCHAIN_PREFIX}gcc}
SKIP_ASM_CONFIG="$SCRIPT_DIR/configs/skip_asm"
NH_LINK_LIBS='-lc -lm -lnosys -Wl,--undefined=SEGGER_RTT_WriteNoLock'
NH_CMAKE_LINK_OVERLAY="$SCRIPT_DIR/../../support/link_anchor.cmake"

cleanup() {
  rm -rf "$TEMP_ROOT"
}
trap cleanup EXIT INT TERM

if test -e "$EVIDENCE_DIR"; then
  echo "NH10 evidence directory already exists: $EVIDENCE_DIR" >&2
  exit 1
fi
mkdir -p "$EVIDENCE_DIR/actual/artifacts" "$EVIDENCE_DIR/actual/logs" \
  "$EVIDENCE_DIR/metadata"

FAILURES="$EVIDENCE_DIR/failures.txt"
: >"$FAILURES"
{
  printf '%s\n' '# NH-10 budgets frozen before execution' ''
  printf '%s\n' '- Minimal current image: Flash <= 16384 B, static RAM <= 2048 B, fixed stack frame <= 256 B.'
  printf '%s\n' '- Current default: Flash <= freshly rebuilt release + 1024 B; static RAM <= release.'
  printf '%s\n' '- Legacy float fast on: Flash <= fast off + 256 B; stack <= fast off + 64 B; RAM unchanged.'
  printf '%s\n' '- Skip ASM: absolute Flash delta versus Skip C <= 256 B; RAM unchanged; M0 remains C-equivalent.'
  printf '%s\n' '- Typed-only and typed-float-only: formatter absent; M0 default/typed: no integer division helper.'
  printf '%s\n' '- Former HW08 build-only profiles are owned here: formatter=default/print_string, typed integer=typed, legacy float=legacy_fast_off/on, RTT Skip=skip_c/skip_asm.'
  printf '%s\n' '- Actual projects: default and globally forced C both link within target regions with unchanged RAM; Skip ASM delta <= 256 B and RAM unchanged.'
  printf '%s\n' '- Every configuration: three clean ELF builds have identical hashes and dimensions.'
} >"$EVIDENCE_DIR/budget_freeze.md"
printf '%s\n' \
  'mcu,project_type,mode,run,text,rodata,data,bss,section_total,gnu_text,gnu_data,gnu_bss,flash,ram,printf_buffer,asm_symbol,c_object_symbol,elf_sha256' \
  >"$EVIDENCE_DIR/actual/measurements.csv"
printf '%s\n' 'mcu,project_type,mode,run,symbol' \
  >"$EVIDENCE_DIR/actual/dependencies.csv"

fail() {
  printf 'NH10 FAIL: %s\n' "$1" | tee -a "$FAILURES" >&2
  exit 1
}

require_file() {
  test -f "$1" || fail "missing artifact $1"
}

section_value() {
  "$ARM_SIZE" -A "$1" | awk -v section="$2" \
    '$1 == section { total += $2 } END { print total + 0 }'
}

find_cmake_artifact() {
  find "$1" -maxdepth 2 -type f -name "$2$3" | head -1
}

record_actual() {
  mcu=$1
  project_type=$2
  mode=$3
  run=$4
  elf=$5
  map=$6
  rtt_object=$7
  config_file=$8
  artifact_key="${mcu}_${project_type}_${mode}"

  text=$(section_value "$elf" .text)
  rodata=$(section_value "$elf" .rodata)
  data=$(section_value "$elf" .data)
  bss=$(section_value "$elf" .bss)
  section_total=$((text + rodata + data + bss))
  set -- $("$ARM_SIZE" "$elf" | awk 'NR == 2 { print $1, $2, $3 }')
  gnu_text=$1
  gnu_data=$2
  gnu_bss=$3
  flash=$((gnu_text + gnu_data))
  ram=$((gnu_data + gnu_bss))
  asm_symbol=$("$ARM_NM" "$elf" | awk \
    '$NF == "SEGGER_RTT_ASM_WriteSkipNoLock" { found=1 } END { print found+0 }')
  c_object_symbol=$("$ARM_NM" "$rtt_object" | awk \
    '$NF == "SEGGER_RTT_WriteSkipNoLock" && $(NF-1) ~ /^[Tt]$/ { found=1 } END { print found+0 }')
  elf_sha=$(shasum -a 256 "$elf" | awk '{ print $1 }')
  printf_buffer=$(awk \
    '$1 == "#define" && $2 == "SEGGER_RTT_PRINTF_BUFFER_SIZE" { value=$3 } END { gsub(/[^0-9]/, "", value); print value+0 }' \
    "$config_file")
  if test "$printf_buffer" -eq 0; then
    user_cfg=$(awk \
      '$1 == "#define" && $2 == "RTT_USER_CFG_ENABLE" { value=$3 } END { gsub(/[^0-9]/, "", value); print value+0 }' \
      "$config_file")
    if test "$user_cfg" -eq 0; then
      printf_buffer=64
    fi
  fi
  test "$printf_buffer" -gt 0 || fail "invalid printf buffer in $config_file"

  printf '%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n' \
    "$mcu" "$project_type" "$mode" "$run" "$text" "$rodata" "$data" "$bss" \
    "$section_total" "$gnu_text" "$gnu_data" "$gnu_bss" "$flash" "$ram" \
    "$printf_buffer" "$asm_symbol" "$c_object_symbol" "$elf_sha" \
    >>"$EVIDENCE_DIR/actual/measurements.csv"

  "$ARM_NM" -u "$elf" | awk '{ print $NF }' | while IFS= read -r symbol; do
    test -n "$symbol" || continue
    printf '%s,%s,%s,%s,%s\n' "$mcu" "$project_type" "$mode" "$run" "$symbol" \
      >>"$EVIDENCE_DIR/actual/dependencies.csv"
  done

  if test "$run" -eq 1; then
    artifact="$EVIDENCE_DIR/actual/artifacts/$artifact_key"
    cp "$elf" "$artifact.elf"
    cp "$map" "$artifact.map"
    "$ARM_OBJDUMP" -h "$elf" >"$artifact.sections.txt"
    "$ARM_NM" "$elf" >"$artifact.symbols.txt"
    "$ARM_NM" "$rtt_object" >"$artifact.rtt_object_symbols.txt"
  fi
  printf 'NH10 actual: %s %s %s run %s Flash=%s RAM=%s ASM=%s Cobj=%s\n' \
    "$mcu" "$project_type" "$mode" "$run" "$flash" "$ram" \
    "$asm_symbol" "$c_object_symbol"
}

make_defs() {
  case "$1" in
    f042) printf '%s' '-DUSE_HAL_DRIVER -DSTM32F042x6' ;;
    f103) printf '%s' '-DUSE_HAL_DRIVER -DSTM32F103xB' ;;
    f411) printf '%s' '-DUSE_HAL_DRIVER -DSTM32F411xE' ;;
    h7b0) printf '%s' '-DUSE_PWR_LDO_SUPPLY -DUSE_HAL_DRIVER -DSTM32H7B0xx' ;;
    *) fail "unknown Make MCU $1" ;;
  esac
}

build_make_actual() {
  mcu=$1
  project=$2
  target=$3
  mode=$4
  run=$5
  build="$TEMP_ROOT/${mcu}_make_${mode}"
  log="$EVIDENCE_DIR/actual/logs/${mcu}_make_${mode}_${run}.log"
  rm -rf "$build"
  if test "$mode" = forced_c; then
    defs="$(make_defs "$mcu") -DRTT_USE_ASM=0"
    make -C "$project" PREFIX="$TOOLCHAIN_PREFIX" -j"$JOBS" \
      BUILD_DIR="$build" DEBUG=0 OPT=-Os C_DEFS="$defs" \
      LIBS="$NH_LINK_LIBS" \
      >"$log" 2>&1 || fail "$mcu Make $mode run $run build"
  elif test "$mode" = skip_asm; then
    make -C "$project" PREFIX="$TOOLCHAIN_PREFIX" -j"$JOBS" \
      BUILD_DIR="$build" DEBUG=0 OPT=-Os \
      LIBS="$NH_LINK_LIBS" \
      RTT_CONFIG_DIR="$SKIP_ASM_CONFIG" >"$log" 2>&1 || \
      fail "$mcu Make $mode run $run build"
  else
    make -C "$project" PREFIX="$TOOLCHAIN_PREFIX" -j"$JOBS" \
      BUILD_DIR="$build" DEBUG=0 OPT=-Os \
      LIBS="$NH_LINK_LIBS" \
      >"$log" 2>&1 || fail "$mcu Make $mode run $run build"
  fi
  elf="$build/$target.elf"
  map="$build/$target.map"
  rtt_object="$build/SEGGER_RTT.o"
  require_file "$elf"
  require_file "$map"
  require_file "$rtt_object"
  config_file="$project/rtt_cfg.h"
  if test "$mode" = skip_asm; then
    config_file="$SKIP_ASM_CONFIG/rtt_cfg.h"
  fi
  test -f "$config_file" || config_file="$project/ARM_SEGGER_RTT/rtt_cfg.h"
  require_file "$config_file"
  record_actual "$mcu" Make "$mode" "$run" "$elf" "$map" "$rtt_object" "$config_file"
}

build_cmake_actual() {
  mcu=$1
  project=$2
  target=$3
  mode=$4
  run=$5
  build="$TEMP_ROOT/${mcu}_cmake_${mode}"
  log="$EVIDENCE_DIR/actual/logs/${mcu}_cmake_${mode}_${run}.log"
  rm -rf "$build"
  if test "$mode" = forced_c; then
    "$CUBE_CMAKE" -S "$project" -B "$build" -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE="$project/cmake/gcc-arm-none-eabi.cmake" \
      -DCMAKE_PROJECT_INCLUDE="$NH_CMAKE_LINK_OVERLAY" \
      -DCMAKE_C_FLAGS=-DRTT_USE_ASM=0 \
      -DCMAKE_ASM_FLAGS=-DRTT_USE_ASM=0 >"$log" 2>&1 || \
      fail "$mcu CMake $mode run $run configure"
  elif test "$mode" = skip_asm; then
    "$CUBE_CMAKE" -S "$project" -B "$build" -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE="$project/cmake/gcc-arm-none-eabi.cmake" \
      -DCMAKE_PROJECT_INCLUDE="$NH_CMAKE_LINK_OVERLAY" \
      -DARM_SEGGER_RTT_CONFIG_DIR="$SKIP_ASM_CONFIG" >"$log" 2>&1 || \
      fail "$mcu CMake $mode run $run configure"
  else
    "$CUBE_CMAKE" -S "$project" -B "$build" -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE="$project/cmake/gcc-arm-none-eabi.cmake" \
      -DCMAKE_PROJECT_INCLUDE="$NH_CMAKE_LINK_OVERLAY" \
      >"$log" 2>&1 || fail "$mcu CMake $mode run $run configure"
  fi
  "$CUBE_CMAKE" --build "$build" --target all -- \
    >>"$log" 2>&1 || fail "$mcu CMake $mode run $run build"
  elf=$(find_cmake_artifact "$build" "$target" '.elf')
  map=$(find_cmake_artifact "$build" "$target" '.map')
  rtt_object=$(find "$build/ARM_SEGGER_RTT" -type f \
    \( -name 'SEGGER_RTT.c.o' -o -name 'SEGGER_RTT.c.obj' \) | head -1)
  require_file "$elf"
  require_file "$map"
  require_file "$rtt_object"
  config_file="$project/rtt_cfg.h"
  if test "$mode" = skip_asm; then
    config_file="$SKIP_ASM_CONFIG/rtt_cfg.h"
  fi
  test -f "$config_file" || config_file="$project/ARM_SEGGER_RTT/rtt_cfg.h"
  require_file "$config_file"
  record_actual "$mcu" CMake "$mode" "$run" "$elf" "$map" "$rtt_object" "$config_file"
}

run_project_pair() {
  mcu=$1
  make_target=$2
  cmake_target=$3
  make_project="$WORKSPACE/${make_target}"
  cmake_project="$WORKSPACE/${cmake_target}"
  for mode in default skip_asm forced_c; do
    for run in 1 2 3; do
      build_make_actual "$mcu" "$make_project" "$make_target" "$mode" "$run"
    done
    for run in 1 2 3; do
      build_cmake_actual "$mcu" "$cmake_project" "$cmake_target" "$mode" "$run"
    done
  done
}

check_actual_results() {
  csv="$EVIDENCE_DIR/actual/measurements.csv"
  awk -F, '
    NR == 1 { next }
    {
      key=$1 FS $2 FS $3
      signature=$5 FS $6 FS $7 FS $8 FS $9 FS $10 FS $11 FS $12 FS $13 FS $14 FS $16 FS $17 FS $18
      count[key]++
      if (!(key in first)) first[key]=signature
      if (first[key] != signature) {
        print "non-repeatable actual build: " key > "/dev/stderr"; bad=1
      }
    }
    END {
      for (key in count) if (count[key] != 3) {
        print "missing actual repetitions: " key > "/dev/stderr"; bad=1
      }
      exit bad
    }
  ' "$csv" || fail 'actual-project repeatability'

  awk -F, '
    NR == 1 || $4 != 1 { next }
    {
      key=$1 FS $2 FS $3
      flash[key]=$13; ram[key]=$14; asm[key]=$16; cobj[key]=$17
    }
    END {
      split("f042 f103 f411 h7b0", mcus, " ")
      split("Make CMake", types, " ")
      for (i in mcus) for (j in types) {
        m=mcus[i]; t=types[j]
        d=m FS t FS "default"; a=m FS t FS "skip_asm"; c=m FS t FS "forced_c"
        if (ram[d] != ram[c] || ram[d] != ram[a]) {
          print m " " t " Skip mode RAM differs" > "/dev/stderr"; bad=1
        }
        if (cobj[c] != 1 || asm[c] != 0) {
          print m " " t " forced C symbols invalid" > "/dev/stderr"; bad=1
        }
        if (asm[d] != 0) {
          print m " " t " default unexpectedly retained ASM" > "/dev/stderr"; bad=1
        }
        delta=flash[a]-flash[d]; if (delta < 0) delta=-delta
        if (delta > 256) {
          print m " " t " Skip ASM Flash delta exceeds 256" > "/dev/stderr"; bad=1
        }
        if (m == "f042") {
          if (flash[d] != flash[a] || cobj[d] != 1 || cobj[a] != 1 || asm[a] != 0) {
            print m " " t " M0 Skip ASM request not C-equivalent" > "/dev/stderr"; bad=1
          }
        } else if (asm[a] != 1 || cobj[a] != 0 || cobj[d] != 0) {
          print m " " t " explicit ASM symbols invalid" > "/dev/stderr"; bad=1
        }
      }
      exit bad
    }
  ' "$csv" || fail 'actual-project ASM/C budget'

  printf '%s\n' 'mcu,project_type,default_flash,skip_asm_flash,skip_asm_minus_default,forced_c_flash,ram' \
    >"$EVIDENCE_DIR/actual/asm_c_delta.csv"
  awk -F, '
    NR == 1 || $4 != 1 { next }
    { key=$1 FS $2; if ($3 == "default") { df[key]=$13; ram[key]=$14 } else if ($3 == "skip_asm") sa[key]=$13; else fc[key]=$13 }
    END { for (key in df) { split(key, p, FS); print p[1] "," p[2] "," df[key] "," sa[key] "," sa[key]-df[key] "," fc[key] "," ram[key] } }
  ' "$csv" | sort >>"$EVIDENCE_DIR/actual/asm_c_delta.csv"
  printf '%s\n' 'All actual-project repeatability, symbol, equivalence, link-capacity, and RAM budgets passed.' \
    >"$EVIDENCE_DIR/actual/budget_results.txt"
}

{
  date '+start=%Y-%m-%d %H:%M:%S %Z (%z)'
  printf 'test_script_sha256=recorded_after_run\n'
  uname -a
  if command -v sw_vers >/dev/null 2>&1; then sw_vers; fi
  make --version | head -1
  "${TOOLCHAIN_PREFIX}gcc" --version | head -1
  "$CUBE_GCC" --version | head -1
  "$CUBE" --version
  "$CUBE_CMAKE" --version
  ninja --version
} >"$EVIDENCE_DIR/metadata/environment.txt" 2>&1

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

echo 'NH10 phase 1/2: minimal configurations and fresh release baseline'
resource_failed=0
if ! sh "$SCRIPT_DIR/resource_usage/run.sh" "$EVIDENCE_DIR/resource_usage"; then
  resource_failed=1
  printf '%s\n' 'minimal/release resource budget failure' >>"$FAILURES"
fi

echo 'NH10 phase 2/2: actual-project default C / Skip ASM / forced C matrix'
run_project_pair f042 stm32f042g6make stm32f042g6cmake
run_project_pair f103 stm32f103c8make stm32f103c8cmake
run_project_pair f411 stm32f411cemake stm32f411cecmake
run_project_pair h7b0 stm32h7b0vbmake stm32h7b0vbcmake
check_actual_results

date '+end=%Y-%m-%d %H:%M:%S %Z (%z)' >>"$EVIDENCE_DIR/metadata/environment.txt"
shasum -a 256 "$SCRIPT_DIR/run.sh" >"$EVIDENCE_DIR/metadata/test_script.sha256"
shasum -a 256 "$SCRIPT_DIR/resource_usage/run.sh" >"$EVIDENCE_DIR/metadata/resource_script.sha256"
find "$EVIDENCE_DIR" -type f ! -name evidence.sha256 -exec shasum -a 256 {} \; | \
  sort >"$EVIDENCE_DIR/evidence.sha256"
if test "$resource_failed" -ne 0 || test -s "$FAILURES"; then
  printf '%s\n' 'NH10 FAIL: one or more frozen resource budgets failed' >&2
  exit 1
fi
printf '%s\n' 'NH10 PASS: release baseline, 3x expanded configurations, 8 actual projects, C/ASM resource comparison'
