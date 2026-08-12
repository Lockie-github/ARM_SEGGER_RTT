#include "rtt_log.h"

int main(void) {
  static const char Probe[] = "probe";

  log_info("ready");
  log_debug("value=%d", -12);
  log_warn("hex=%08x", 0x2Au);
  log_err("text=%s", "error");
  log_print("plain=%u", 7u);
  log_float_label(1.25f, "float");
  (void)SEGGER_RTT_WriteSkipNoLock(0u, Probe, sizeof(Probe) - 1u);
  return 0;
}
