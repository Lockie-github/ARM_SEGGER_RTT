#ifndef HW02_PROJECT_NAME
#error "HW02_PROJECT_NAME must identify the build system"
#endif

#ifndef HW02_BUILD_TYPE
#error "HW02_BUILD_TYPE must identify the build configuration"
#endif

static float HW02_FromBits(uint32_t Bits)
{
  union {
    uint32_t Bits;
    float Value;
  } Convert;

  Convert.Bits = Bits;
  return Convert.Value;
}

static void HW02_Run(void)
{
  log_print("HW-02|BEGIN|STM32H7B0VB|" HW02_PROJECT_NAME "|" HW02_BUILD_TYPE
            "|SHA=" HW_TEST_LIBRARY_SHA "\n");
  log_info("C01|log_info|value=%d", -12);
  log_debug("C02|log_debug|hex=%08X", 0x89ABCDEFu);
  log_warn("C03|log_warn|text=%s", "ready");
  log_err("C04|log_err|unsigned=%u", 4294967295u);
  log_print("C05|log_print|plain=%% %c\n", 'A');
  (void)log_string("C06|log_string|raw text\n");
  (void)log_string("C07A|log_float|finite=");
  log_float(-2.5f);
  (void)log_string("C07B|log_float|nan=");
  log_float(HW02_FromBits(UINT32_C(0x7FC00000)));
  log_float_label("C08A|log_float_label", HW02_FromBits(UINT32_C(0x7F800000)));
  (void)log_string("C08B|log_float_label|null-label=");
  log_float_label((const char *)0, HW02_FromBits(UINT32_C(0xFF800000)));
  log_i32("C09|log_i32", INT32_MIN);
  (void)log_string("C10|log_u32|empty-label=");
  log_u32("", UINT32_MAX);
  log_hex32("C11|log_hex32", UINT32_C(0x89ABCDEF));
  (void)log_string("C12|log_pointer|null-label=");
  log_pointer((const char *)0, (const void *)0);
  log_f32("C13A|log_f32", 1.25f);
  (void)log_string("C13B|log_f32|empty-label=");
  log_f32("", HW02_FromBits(UINT32_C(0x7FC00000)));
  (void)log_string("HW-02|END|MACROS=13|RECORDS=16\n");
}
