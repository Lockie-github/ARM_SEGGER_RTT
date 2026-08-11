# RTT log call-site Flash benchmark

This benchmark compares two logging front ends on Cortex-M0:

- `direct`: every call site contains the ANSI color, level tag, reset, and newline.
- `collected`: call sites contain only the user format string; a shared
  `RTT_LogPrintf()` implementation owns the framing strings.

The RTT transport is common to both designs and is excluded. The collected
result includes the complete shared wrapper, all four colored level prefixes,
and the measured size delta between the framed formatter and the formatter at
baseline commit `dadde11`.

Run with the Arm GNU Toolchain 12.2.1 used by the repository size reports:

```sh
./benchmarks/rtt_log_flash/run.sh
```

Override the toolchain when needed:

```sh
TOOLCHAIN_PREFIX=/path/to/bin/arm-none-eabi- \
  ./benchmarks/rtt_log_flash/run.sh
```

## Measured results

Toolchain: Arm GNU Toolchain 12.2.1, Cortex-M0, color enabled, no LTO.

| Optimization | Log sites | Direct | Collected | Saving |
|---|---:|---:|---:|---:|
| `-O0` | 10 | 714 B | 891 B | -177 B (-24.79%) |
| `-O0` | 50 | 3,598 B | 2,975 B | 623 B (17.32%) |
| `-O0` | 100 | 7,198 B | 5,575 B | 1,623 B (22.55%) |
| `-Os` | 10 | 648 B | 684 B | -36 B (-5.56%) |
| `-Os` | 50 | 3,288 B | 2,564 B | 724 B (22.02%) |
| `-Os` | 100 | 6,592 B | 4,918 B | 1,674 B (25.39%) |

At `-Os` with 100 sites, call-site code is 1,992 B in both variants.
User format strings fall from 4,600 B to 2,700 B, while the complete collected
wrapper, four colored prefixes, reset, and newline cost 163 B. The framed
formatter adds another 63 B at `-Os` (90 B at `-O0`). After both costs are
included, the measured saving still comes from deduplicating framing strings,
not from reducing call instructions.
