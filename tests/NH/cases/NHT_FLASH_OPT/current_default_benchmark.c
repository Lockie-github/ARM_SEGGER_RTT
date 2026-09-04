#include "rtt_log.h"

volatile unsigned g_resource_value = 123u;
volatile float g_resource_float = -12.345f;

int main(void) {
  log_info("info=%u", g_resource_value);
  log_debug("debug=%x", g_resource_value);
  log_warn("warn=%d", (int)g_resource_value);
  log_err("error=%s", "text");
  log_print("print=%u", g_resource_value);
  (void)log_string("string");
  log_float(g_resource_float);
  log_float_label("float", g_resource_float);
  return (int)(g_resource_value & 0u);
}
