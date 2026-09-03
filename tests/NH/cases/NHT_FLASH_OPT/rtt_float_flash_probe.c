#include "rtt_log.h"

#if defined(__GNUC__) || defined(__clang__)
  #define TEST_UNUSED __attribute__((unused))
#else
  #define TEST_UNUSED
#endif

int main(void) {
  static const char Raw[] TEST_UNUSED = "raw";
  static const char Skip[] = "skip";

  log_info("info=%d", 1);
  log_debug("debug=%d", 2);
  log_warn("warn=%d", 3);
  log_err("error=%d", 4);
  log_print("print=%d", 5);
  (void)log_string(Raw);
  log_float(1.25f);
  log_float_label("float", -2.5f);
  (void)SEGGER_RTT_WriteSkipNoLock(RTT_LOG_BUFFER_INDEX,
                                   Skip,
                                   (unsigned)(sizeof(Skip) - 1u));
  return 0;
}

#undef TEST_UNUSED
