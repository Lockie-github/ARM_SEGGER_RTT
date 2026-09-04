#include "rtt_log.h"

int main(void)
{
  static const char probe[] = "probe";

  log_info("ready");
  log_debug("value=%d", -12);
  log_warn("hex=%08x", 0x2Au);
  log_err("text=%s", "error");
  log_print("plain=%u\n", 7u);
  log_float_label("float", 1.25f);
  log_i32("i32", -1);
  log_f32("f32", 1.25f);
  (void)SEGGER_RTT_WriteSkipNoLock(0u, probe, sizeof(probe) - 1u);
  return 0;
}
