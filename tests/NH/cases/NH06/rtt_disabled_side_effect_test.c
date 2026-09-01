#include "rtt_log.h"

static int SideEffects;

#if defined(__GNUC__) || defined(__clang__)
  #define TEST_UNUSED __attribute__((unused))
#else
  #define TEST_UNUSED
#endif

static TEST_UNUSED int side_effect_int(void) {
  SideEffects++;
  return 7;
}

static TEST_UNUSED const char * side_effect_text(void) {
  SideEffects++;
  return "text";
}

int main(void) {
  log_info("%d", side_effect_int());
  log_debug("%d", side_effect_int());
  log_warn("%d", side_effect_int());
  log_err("%d", side_effect_int());
  log_print("%d", side_effect_int());
  (void)log_string(side_effect_text());
  log_float((float)side_effect_int());
  log_float_label(side_effect_text(), (float)side_effect_int());
  return SideEffects == 0 ? 0 : 1;
}

#undef TEST_UNUSED
