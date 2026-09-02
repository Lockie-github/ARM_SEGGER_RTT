# NH-10 static resource measurement

This fixture performs fresh `-Os`, non-LTO, garbage-collected Cortex-M builds
with the Arm GNU Toolchain selected by `tests/NH/config.local.json` or the
runner command line. The exact compiler and tool versions are recorded in the
evidence, and resource comparisons are scoped to that recorded environment.
Every configuration is built in three new directories. The test fails if the
three ELF hashes differ.

The Cortex-M0, M3, M4F, and M7 matrix covers an empty baseline, default,
Lite, print+string only, typed integers, typed float, the typed combination,
legacy float fast off/on, and Skip C/ASM. An equivalent full `release` build
is rebuilt for every target. The `release` sources are extracted from Git
during the run; historical numbers are not read.

This matrix is also the sole owner of the former HW08 build-only resource
profiles. Their mapping is: formatter to `default`/`print_string`, typed
integer to `typed`, legacy float to `legacy_fast_off`/`legacy_fast_on`, and
RTT Skip to `skip_c`/`skip_asm`. These checks belong here because they require
ELF, map, size, symbol, and stack-usage analysis but do not require a board.

Budgets are frozen before measurement:

- Every current minimal image must remain within 16 KiB Flash, 2 KiB static
  RAM, and a 256 B maximum fixed stack frame.
- Current default may exceed the freshly rebuilt equivalent `release` image by
  at most 1024 B Flash; it must not increase static RAM.
- Enabling legacy float fast mode may add at most 256 B Flash and 64 B fixed
  stack versus fast-off, and must not increase static RAM.
- Enabling Skip ASM may change Flash by at most 256 B versus Skip C and must
  not increase static RAM. Cortex-M0 must remain byte-equivalent C code.
- Typed-only and typed-float-only images must not retain the formatter.
- All three clean builds of a configuration must have identical ELF hashes.
- Cortex-M0 default and typed paths must not introduce an `__aeabi_*div*`
  symbol; all undefined and selected runtime symbols are reported.

The actual-project ASM/C comparison uses these additional budgets, frozen
before the 2026-08-21 NH-10 run:

- Every Make/CMake mode must link within the target memory regions, and three
  clean Release builds must produce identical ELF hashes and section sizes.
- All applications explicitly default to Skip C; default and globally forced
  C must both link within the target regions and keep static RAM unchanged.
- A separate config enables Skip ASM. It must remain C-equivalent on F042 and
  resolve the ASM symbol on F103/F411/H7B0.
- Skip ASM must not change static RAM and may change Flash by at most 256 B.
- Successful target linking is the absolute actual-project capacity budget.

Run through the unified runner for evidence and candidate identity tracking:

```sh
python3 tests/NH/run_nh.py --case NH10
```

Direct execution of `tests/NH/cases/NH10/run.sh` is only for fixture
development and does not form formal release evidence.
