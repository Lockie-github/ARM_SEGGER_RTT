#include "rtt_log.h"

volatile unsigned g_resource_value = 123u;
volatile float g_resource_float = -12.345f;

int main(void) {
#if RESOURCE_PROFILE == 1
  log_info("info=%u", g_resource_value);
  log_debug("debug=%x", g_resource_value);
  log_warn("warn=%d", (int)g_resource_value);
  log_err("error=%s", "text");
  log_print("print=%u", g_resource_value);
  log_float(g_resource_float);
  log_float_desc("float", g_resource_float);
#elif RESOURCE_PROFILE == 4
  log_float(g_resource_float);
  log_float_desc("float", g_resource_float);
#endif
  return (int)(g_resource_value & 0u);
}
