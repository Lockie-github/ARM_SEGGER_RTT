extern int SEGGER_RTT_printf(unsigned BufferIndex, const char * pFormat, ...);

typedef enum {
  RTT_LOG_LEVEL_INFO,
  RTT_LOG_LEVEL_DEBUG,
  RTT_LOG_LEVEL_WARN,
  RTT_LOG_LEVEL_ERROR
} RTT_LOG_LEVEL;

extern int RTT_LogPrintf(RTT_LOG_LEVEL Level, const char * pFormat, ...);

#ifndef BENCH_COUNT
  #define BENCH_COUNT 10
#endif

#define BENCH_STRINGIFY_INNER(Value) #Value
#define BENCH_STRINGIFY(Value)       BENCH_STRINGIFY_INNER(Value)

#if defined(BENCH_COLLECTED)
  #define log_info(...) \
    ((void)RTT_LogPrintf(RTT_LOG_LEVEL_INFO, __VA_ARGS__))
#else
  #define log_info(Format, ...)                                                \
    ((void)SEGGER_RTT_printf(0u,                                              \
                             "\x1B[1;32m[INFO] " Format "\x1B[0m\n",       \
                             ##__VA_ARGS__))
#endif

#define BENCH_SITE(Block, Slot)                                                \
  log_info("site-" BENCH_STRINGIFY(Block) "-" BENCH_STRINGIFY(Slot)          \
           " value=%d hex=%08x",                                             \
           Value + ((Block) * 10) + (Slot),                                   \
           (unsigned)(Value ^ (((Block) * 10) + (Slot))))

#define BENCH_BLOCK(Block) \
  BENCH_SITE(Block, 0);     \
  BENCH_SITE(Block, 1);     \
  BENCH_SITE(Block, 2);     \
  BENCH_SITE(Block, 3);     \
  BENCH_SITE(Block, 4);     \
  BENCH_SITE(Block, 5);     \
  BENCH_SITE(Block, 6);     \
  BENCH_SITE(Block, 7);     \
  BENCH_SITE(Block, 8);     \
  BENCH_SITE(Block, 9)

void RTT_LogFlashBenchmark(int Value) {
  BENCH_BLOCK(0);
#if BENCH_COUNT >= 50
  BENCH_BLOCK(1);
  BENCH_BLOCK(2);
  BENCH_BLOCK(3);
  BENCH_BLOCK(4);
#endif
#if BENCH_COUNT >= 100
  BENCH_BLOCK(5);
  BENCH_BLOCK(6);
  BENCH_BLOCK(7);
  BENCH_BLOCK(8);
  BENCH_BLOCK(9);
#endif
}
