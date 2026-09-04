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
  (void)log_string("string");
  log_float(g_resource_float);
  log_float_label("float", g_resource_float);
#elif RESOURCE_PROFILE == 2
  (void)log_string("string");
#elif RESOURCE_PROFILE == 3
  log_print("print=%u", g_resource_value);
#elif RESOURCE_PROFILE == 4
  log_float(g_resource_float);
  log_float_label("float", g_resource_float);
#elif RESOURCE_PROFILE == 5
  log_i32("i32", (int32_t)g_resource_value);
  log_u32("u32", (uint32_t)g_resource_value);
  log_hex32("hex", (uint32_t)g_resource_value);
  log_pointer("ptr", (const void *)&g_resource_value);
#elif RESOURCE_PROFILE == 6
  log_f32("f32", g_resource_float);
#elif RESOURCE_PROFILE == 7
  log_i32("i32", (int32_t)g_resource_value);
  log_u32("u32", (uint32_t)g_resource_value);
  log_hex32("hex", (uint32_t)g_resource_value);
  log_pointer("ptr", (const void *)&g_resource_value);
  log_f32("f32", g_resource_float);
#elif RESOURCE_PROFILE == 8
  log_print("print=%u", g_resource_value);
  (void)log_string("string");
#endif
  return (int)(g_resource_value & 0u);
}
