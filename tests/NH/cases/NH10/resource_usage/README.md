# NH-10 static resource measurement

This fixture performs fresh `-Os`, non-LTO, garbage-collected Cortex-M builds
with the Arm GNU Toolchain selected by `tests/NH/config.local.json` or the
runner command line. The exact compiler and tool versions are recorded in the
evidence, and resource comparisons are scoped to that recorded environment.
Every configuration is built in three new directories. The test fails if the
three ELF hashes differ.

The Cortex-M0, M3, M4F, and M7 matrix covers default, Lite, print+string only,
typed integers, typed float, the typed combination, legacy float fast off/on,
and Skip C/ASM. Each configuration links the identical benchmark object twice:
once against zero-sized control symbols and once against RTT. Historical branch
comparisons are not part of NH10; temporary Flash optimization comparisons live
in `NHT_FLASH_OPT`.

This matrix is also the sole owner of the former HW08 build-only resource
profiles. Their mapping is: formatter to `default`/`print_string`, typed
integer to `typed`, legacy float to `legacy_fast_off`/`legacy_fast_on`, and
RTT Skip to `skip_c`/`skip_asm`. These checks belong here because they require
ELF, map, size, symbol, and stack-usage analysis but do not require a board.

Budgets are frozen before measurement:

- Every current configuration must remain within 2 KiB static RAM and a 256 B
  maximum fixed stack frame. `library_footprint.csv` reports the linked library
  Flash footprint as the full RTT link minus the paired control link built from
  the identical benchmark object. This excludes the benchmark's call sites,
  globals, and string literals while including retained RTT code, data
  initializers, and transitive runtime support pulled in by the configured API
  use; it has no architecture-independent absolute cap.
- `flash_footprint.md` is the human-readable Flash report. It contains two
  tables, `default` followed by `typed_combo`, with the full link, paired
  control, and linked library Flash values for all four architectures. The CSV
  remains the complete machine-readable result for every measured profile.
  `default` enables and exercises the four level APIs, print, string, and
  legacy float. `typed_combo` enables and exercises typed integer/pointer and
  typed float only. Values are bytes.
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
development and does not form formal release evidence. NH10 writes evidence
only; after the complete NH/HW evidence set is accepted, the release closeout
uses `flash_footprint.md` to fill the matching two tables in
`docs/quality/test_report.md`.
