#!/bin/sh
set -eu

LC_ALL=C
export LC_ALL

TOOLCHAIN_PREFIX=${TOOLCHAIN_PREFIX:-arm-none-eabi-}
RESOURCE_SCOPE=${NH10_RESOURCE_SCOPE:-full}
OUTPUT_DIR=${1:-resource_results}
CC=${TOOLCHAIN_PREFIX}gcc
SIZE=${TOOLCHAIN_PREFIX}size
NM=${TOOLCHAIN_PREFIX}nm
OBJDUMP=${TOOLCHAIN_PREFIX}objdump
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/../../../../.." && pwd)
WORK_DIR=$(mktemp -d "${TMPDIR:-/tmp}/rtt-resource.XXXXXX")

case "$RESOURCE_SCOPE" in
  full) ;;
  *)
    printf 'unsupported NH10_RESOURCE_SCOPE: %s\n' "$RESOURCE_SCOPE" >&2
    exit 2
    ;;
esac

cleanup() {
  rm -rf "$WORK_DIR"
}
trap cleanup EXIT INT TERM

mkdir -p "$OUTPUT_DIR/artifacts"
OUTPUT_DIR=$(CDPATH= cd -- "$OUTPUT_DIR" && pwd)
FAILURES="$OUTPUT_DIR/failures.txt"
: > "$FAILURES"
"$CC" --version | sed -n '1p' > "$OUTPUT_DIR/toolchain.txt"
git -C "$PROJECT_DIR" rev-parse HEAD > "$OUTPUT_DIR/current_commit.txt"

printf '%s\n' 'implementation,configuration,target,run,text,rodata,data,bss,flash,ram,max_stack,elf_sha256' > "$OUTPUT_DIR/measurements.csv"
printf '%s\n' 'implementation,configuration,target,run,symbol' > "$OUTPUT_DIR/dependencies.csv"
printf '%s\n' 'implementation,configuration,target,run,symbol' > "$OUTPUT_DIR/undefined_symbols.csv"
printf '%s\n' 'implementation,configuration,target,run,formatter,asm_symbol' > "$OUTPUT_DIR/properties.csv"

common_flags='-std=gnu11 -Os -g0 -ffunction-sections -fdata-sections -fno-lto -fstack-usage -Wall -Wextra -Werror'
common_link='--specs=nosys.specs -nostartfiles -Wl,-e,main -Wl,--gc-sections'
control_symbols='-Wl,--defsym,RTT_LogFloat3=0 -Wl,--defsym,RTT_LogPrintf=0 -Wl,--defsym,RTT_LogString=0 -Wl,--defsym,SEGGER_RTT_printf=0 -Wl,--defsym,RTT_LogI32=0 -Wl,--defsym,RTT_LogU32=0 -Wl,--defsym,RTT_LogHex32=0 -Wl,--defsym,RTT_LogPointer=0 -Wl,--defsym,RTT_LogF32=0'

section_value() {
  "$SIZE" -A "$1" | awk -v section="$2" '$1 == section { total += $2 } END { print total + 0 }'
}

max_stack() {
  build=$1
  elf=$2
  "$NM" --defined-only "$elf" | awk 'NF >= 3 { print $3 }' > "$build/linked-symbols.txt"
  find "$build" -name '*.su' -type f -exec awk -F '\t' \
    'NR == FNR { linked[$1] = 1; next }
     NF >= 2 && $2 ~ /^[0-9]+$/ {
       count = split($1, parts, ":");
       name = parts[count];
       if (name in linked) print $2;
     }' "$build/linked-symbols.txt" {} \; | \
    awk 'BEGIN { max = 0 } $1 > max { max = $1 } END { print max }'
}

write_config() {
  cfg_file=$1
  profile=$2
  lite=0
  log_enable=1
  info=0
  debug=0
  warn=0
  error=0
  print_enable=0
  string_enable=0
  float_enable=0
  typed_enable=0
  typed_float_enable=0
  use_modff=0
  float_fast_path=0
  skip_asm=0

  case "$profile" in
    default)
      info=1; debug=1; warn=1; error=1; print_enable=1; string_enable=1; float_enable=1 ;;
    lite)
      lite=1; info=1; debug=1; warn=1; error=1; print_enable=1; string_enable=1; float_enable=1 ;;
    print_string)
      print_enable=1; string_enable=1 ;;
    typed)
      typed_enable=1 ;;
    typed_float)
      typed_float_enable=1 ;;
    typed_combo)
      typed_enable=1; typed_float_enable=1 ;;
    legacy_fast_off)
      float_enable=1 ;;
    legacy_fast_on)
      float_enable=1; float_fast_path=1 ;;
    skip_c)
      string_enable=1 ;;
    skip_asm)
      string_enable=1; skip_asm=1 ;;
    *)
      echo "unknown profile: $profile" >&2
      exit 1 ;;
  esac

  {
    printf '%s\n' '#ifndef RESOURCE_RTT_CFG_H' '#define RESOURCE_RTT_CFG_H'
    printf '%s\n' '#define RTT_USER_CFG_ENABLE 1'
    printf '#define RTT_LOG_ENABLE %s\n' "$log_enable"
    printf '#define LOG_ENABLE_LITE %s\n' "$lite"
    printf '#define LOG_ENABLE_TYPED %s\n' "$typed_enable"
    printf '#define LOG_ENABLE_TYPED_FLOAT %s\n' "$typed_float_enable"
    printf '#define LOG_ENABLE_INFO %s\n' "$info"
    printf '#define LOG_ENABLE_DEBUG %s\n' "$debug"
    printf '#define LOG_ENABLE_WARN %s\n' "$warn"
    printf '#define LOG_ENABLE_ERROR %s\n' "$error"
    printf '#define LOG_ENABLE_PRINT %s\n' "$print_enable"
    printf '#define LOG_ENABLE_STRING %s\n' "$string_enable"
    printf '#define LOG_ENABLE_FLOAT %s\n' "$float_enable"
    printf '#define RTT_FLOAT_USE_MODFF %s\n' "$use_modff"
    printf '#define RTT_LOG_FLOAT_FAST_PATH %s\n' "$float_fast_path"
    printf '#define RTT_WRITE_SKIP_USE_ASM %s\n' "$skip_asm"
    printf '%s\n' '#define RTT_LOG_BUFFER_INDEX 0u' '#define RTT_LOG_USE_COLOR 1'
    printf '%s\n' '#define SEGGER_RTT_MAX_NUM_UP_BUFFERS 3' '#define SEGGER_RTT_MAX_NUM_DOWN_BUFFERS 3'
    printf '%s\n' '#define BUFFER_SIZE_UP 1024' '#define BUFFER_SIZE_DOWN 16' '#define SEGGER_RTT_PRINTF_BUFFER_SIZE 64u'
    printf '%s\n' '#endif'
  } > "$cfg_file"
}

build_current() {
  profile=$1
  target=$2
  run=$3
  arch_flags=$4
  profile_id=$5
  build="$WORK_DIR/current_${profile}_${target}_${run}"
  mkdir -p "$build/config"
  write_config "$build/config/rtt_cfg.h" "$profile"

  "$CC" $common_flags $arch_flags -DRESOURCE_PROFILE="$profile_id" \
    -I"$build/config" -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
    -c "$SCRIPT_DIR/current_benchmark.c" -o "$build/benchmark.o"
  "$CC" $common_flags $arch_flags $common_link $control_symbols \
    -Wl,-Map="$build/control.map" "$build/benchmark.o" \
    -lc -lnosys -o "$build/control.elf"
  record_result control "$profile" "$target" "$run" "$build" control
  "$CC" $common_flags $arch_flags -I"$build/config" -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
    -c "$PROJECT_DIR/RTT/SEGGER_RTT.c" -o "$build/SEGGER_RTT.o"
  "$CC" $common_flags $arch_flags -I"$build/config" -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
    -c "$PROJECT_DIR/rtt_printf.c" -o "$build/rtt_printf.o"
  "$CC" $common_flags $arch_flags -I"$build/config" -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
    -c "$PROJECT_DIR/rtt_log.c" -o "$build/rtt_log.o"
  "$CC" $common_flags $arch_flags -I"$build/config" -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
    -c "$PROJECT_DIR/rtt_float.c" -o "$build/rtt_float.o"
  "$CC" $common_flags $arch_flags -x assembler-with-cpp -I"$build/config" -I"$PROJECT_DIR" -I"$PROJECT_DIR/RTT" \
    -c "$PROJECT_DIR/RTT/SEGGER_RTT_ASM_ARMv7M.S" -o "$build/SEGGER_RTT_ASM_ARMv7M.o"
  "$CC" $common_flags $arch_flags $common_link -Wl,-Map="$build/result.map" \
    "$build/benchmark.o" "$build/SEGGER_RTT.o" "$build/rtt_printf.o" \
    "$build/rtt_log.o" "$build/rtt_float.o" "$build/SEGGER_RTT_ASM_ARMv7M.o" \
    -lm -lc -lnosys -o "$build/result.elf"
  record_result current "$profile" "$target" "$run" "$build" result
}

record_result() {
  implementation=$1
  profile=$2
  target=$3
  run=$4
  build=$5
  stem=$6
  elf="$build/$stem.elf"
  text=$(section_value "$elf" .text)
  rodata=$(section_value "$elf" .rodata)
  data=$(section_value "$elf" .data)
  bss=$(section_value "$elf" .bss)
  flash=$((text + rodata + data))
  ram=$((data + bss))
  stack=$(max_stack "$build" "$elf")
  sha=$(shasum -a 256 "$elf" | awk '{ print $1 }')
  printf '%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n' \
    "$implementation" "$profile" "$target" "$run" "$text" "$rodata" "$data" "$bss" \
    "$flash" "$ram" "$stack" "$sha" >> "$OUTPUT_DIR/measurements.csv"
  "$NM" "$elf" | awk '/modff|__aeabi_[a-z]*div/ { print $3 }' | while IFS= read -r symbol; do
    printf '%s,%s,%s,%s,%s\n' "$implementation" "$profile" "$target" "$run" "$symbol" >> "$OUTPUT_DIR/dependencies.csv"
  done
  "$NM" -u "$elf" | awk '{ print $NF }' | while IFS= read -r symbol; do
    test -n "$symbol" || continue
    printf '%s,%s,%s,%s,%s\n' "$implementation" "$profile" "$target" "$run" "$symbol" >> "$OUTPUT_DIR/undefined_symbols.csv"
  done
  formatter=$("$NM" "$elf" | awk '$NF ~ /^RTT_vprintf/ { found=1 } END { print found+0 }')
  asm_symbol=$("$NM" "$elf" | awk '$NF == "SEGGER_RTT_ASM_WriteSkipNoLock" { found=1 } END { print found+0 }')
  printf '%s,%s,%s,%s,%s,%s\n' "$implementation" "$profile" "$target" "$run" "$formatter" "$asm_symbol" >> "$OUTPUT_DIR/properties.csv"
  if [ "$run" -eq 1 ]; then
    artifact_dir="$OUTPUT_DIR/artifacts/${implementation}_${profile}_${target}"
    mkdir -p "$artifact_dir"
    artifact="$artifact_dir/result"
    cp "$elf" "${artifact}.elf"
    cp "$build/$stem.map" "${artifact}.map"
    "$OBJDUMP" -h "$elf" > "${artifact}.sections.txt"
    "$OBJDUMP" -d "$elf" > "${artifact}.objdump.txt"
    "$NM" "$elf" > "${artifact}.symbols.txt"
    "$NM" -u "$elf" > "${artifact}.undefined.txt"
    find "$build" -maxdepth 1 -name '*.o' -type f -exec cp {} "$artifact_dir" \;
    find "$build" -name '*.su' -type f -exec cp {} "$artifact_dir" \;
  fi
}

run_target_matrix() {
  target=$1
  arch_flags=$2
  for run in 1 2 3; do
    build_current default "$target" "$run" "$arch_flags" 1
    build_current lite "$target" "$run" "$arch_flags" 1
    build_current print_string "$target" "$run" "$arch_flags" 8
    build_current typed "$target" "$run" "$arch_flags" 5
    build_current typed_float "$target" "$run" "$arch_flags" 6
    build_current typed_combo "$target" "$run" "$arch_flags" 7
    build_current legacy_fast_off "$target" "$run" "$arch_flags" 4
    build_current legacy_fast_on "$target" "$run" "$arch_flags" 4
    build_current skip_c "$target" "$run" "$arch_flags" 2
    build_current skip_asm "$target" "$run" "$arch_flags" 2
  done
}

run_target_matrix cortex-m0 '-mcpu=cortex-m0 -mthumb'
run_target_matrix cortex-m3 '-mcpu=cortex-m3 -mthumb'
run_target_matrix cortex-m4f '-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard'
run_target_matrix cortex-m7 '-mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard'

if ! awk -F, 'NR == 1 { next } { key=$1 FS $2 FS $3; hash[key]=hash[key] " " $12; count[key]++ } END { for (key in count) { if (count[key] != 3) { print "missing repetitions: " key > "/dev/stderr"; bad=1 } split(hash[key], h, " "); if (h[1] != h[2] || h[1] != h[3]) { print "non-deterministic ELF: " key > "/dev/stderr"; bad=1 } } exit bad }' "$OUTPUT_DIR/measurements.csv"; then
  printf '%s\n' 'repeatability failure' >> "$FAILURES"
fi

if ! awk -F, '
  NR == 1 || $4 != 1 { next }
  { key=$1 FS $2 FS $3; flash[key]=$9; ram[key]=$10; stack[key]=$11 }
  END {
    split("cortex-m0 cortex-m3 cortex-m4f cortex-m7", targets, " ");
    split("default lite print_string typed typed_float typed_combo legacy_fast_off legacy_fast_on skip_c skip_asm", profiles, " ");
    for (i in targets) {
      target=targets[i];
      for (j in profiles) {
        key="current," profiles[j] "," target;
        if (ram[key] > 2048 || stack[key] > 256) {
          print key " exceeds RAM or stack budget" > "/dev/stderr"; bad=1;
        }
      }
      off="current,legacy_fast_off," target; on="current,legacy_fast_on," target;
      if (flash[on] > flash[off] + 256 || ram[on] != ram[off] || stack[on] > stack[off] + 64) {
        print target " fast-path delta exceeds budget" > "/dev/stderr"; bad=1;
      }
      c="current,skip_c," target; a="current,skip_asm," target;
      delta=flash[a]-flash[c]; if (delta < 0) delta=-delta;
      if (delta > 256 || ram[a] != ram[c]) {
        print target " Skip ASM/C delta exceeds budget" > "/dev/stderr"; bad=1;
      }
    }
    exit bad;
  }
' "$OUTPUT_DIR/measurements.csv"; then
  printf '%s\n' 'RAM, stack, or relative resource budget failure' >> "$FAILURES"
fi

if awk -F, '$1 == "current" && $3 == "cortex-m0" && ($2 == "default" || $2 ~ /^typed/) && $5 ~ /^__aeabi_.*div/ { found=1 } END { exit found ? 0 : 1 }' "$OUTPUT_DIR/dependencies.csv"; then
  echo 'current Cortex-M0 default/typed build contains a forbidden division dependency' >&2
  printf '%s\n' 'current Cortex-M0 default/typed division dependency' >> "$FAILURES"
fi

if ! awk -F, '
  NR == 1 || $1 != "current" || $4 != 1 { next }
  $2 == "typed" || $2 == "typed_float" || $2 == "typed_combo" { if ($5 != 0) bad=1 }
  $2 == "skip_c" { if ($6 != 0) bad=1 }
  $2 == "skip_asm" && $3 == "cortex-m0" { if ($6 != 0) bad=1 }
  $2 == "skip_asm" && $3 != "cortex-m0" { if ($6 != 1) bad=1 }
  END { exit bad }
' "$OUTPUT_DIR/properties.csv"; then
  printf '%s\n' 'formatter or Skip symbol budget failure' >> "$FAILURES"
fi

printf '%s\n' \
  'target,configuration,final_flash,control_flash,library_flash,final_ram,control_ram,library_ram,max_stack' \
  >"$OUTPUT_DIR/library_footprint.csv"
awk -F, '
  NR == 1 || $4 != 1 { next }
  {
    key=$3 FS $2
    target[key]=$3; profile[key]=$2
    if ($1 == "control") { control_flash[key]=$9; control_ram[key]=$10 }
    if ($1 == "current") { flash[key]=$9; ram[key]=$10; stack[key]=$11 }
  }
  END {
    for (key in target) {
      print target[key] "," profile[key] "," flash[key] "," control_flash[key] "," \
        flash[key]-control_flash[key] "," ram[key] "," control_ram[key] "," \
        ram[key]-control_ram[key] "," stack[key]
    }
  }
' "$OUTPUT_DIR/measurements.csv" | sort >>"$OUTPUT_DIR/library_footprint.csv"

if ! awk -F, '
  function print_table(configuration, title,    i, key) {
    print "## " title
    print ""
    print "| Target | Full link (B) | Paired control (B) | Library Flash (B) |"
    print "|---|---:|---:|---:|"
    for (i = 1; i <= target_count; i++) {
      key=configuration SUBSEP targets[i]
      if (!(key in library_flash)) {
        print "missing Flash result: " configuration "," targets[i] > "/dev/stderr"
        bad=1
        continue
      }
      print "| " targets[i] " | " final_flash[key] " | " control_flash[key] " | " library_flash[key] " |"
    }
    print ""
  }
  BEGIN {
    target_count=split("cortex-m0 cortex-m3 cortex-m4f cortex-m7", targets, " ")
    print "# NH10 linked library Flash footprint"
    print ""
    print "Library Flash = full RTT link Flash - paired control link Flash."
    print "The default profile exercises level logs, print, string, and legacy float."
    print "The typed_combo profile exercises typed integer/pointer and typed float only."
    print ""
  }
  NR == 1 { next }
  $2 == "typed_combo" || $2 == "default" {
    key=$2 SUBSEP $1
    final_flash[key]=$3
    control_flash[key]=$4
    library_flash[key]=$5
  }
  END {
    print_table("default", "default")
    print_table("typed_combo", "typed_combo")
    exit bad
  }
' "$OUTPUT_DIR/library_footprint.csv" >"$OUTPUT_DIR/flash_footprint.md"; then
  printf '%s\n' 'default/typed_combo Flash report failure' >> "$FAILURES"
fi

if grep -q '^repeatability failure$' "$FAILURES"; then
  printf '%s\n' 'One or more configurations were not repeatable.' > "$OUTPUT_DIR/repeatability.txt"
else
  printf '%s\n' 'All configurations produced identical ELF hashes across three clean builds.' > "$OUTPUT_DIR/repeatability.txt"
fi
if test -s "$FAILURES"; then
  cp "$FAILURES" "$OUTPUT_DIR/budget_results.txt"
  echo "resource evidence completed with failures: $OUTPUT_DIR" >&2
  exit 1
fi
printf '%s\n' 'All RAM, stack, relative resource, and dependency budgets passed; linked library Flash footprints were recorded without an absolute cap.' > "$OUTPUT_DIR/budget_results.txt"
echo "resource evidence: $OUTPUT_DIR"
