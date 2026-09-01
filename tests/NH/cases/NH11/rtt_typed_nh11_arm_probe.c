#include "rtt_log.c"

_Static_assert(sizeof(uintptr_t) == 4u,
               "NH11 Arm probe requires 32-bit pointers");
_Static_assert(RTT_TYPED_POINTER_SIZE == 11u,
               "32-bit pointer text must contain 0x, 8 digits, and newline");
_Static_assert(RTT_TYPED_LOG_FRAME_SIZE >= 59u,
               "typed frame must hold a 46-byte label and 32-bit pointer");

static volatile int ResultSink;

unsigned SEGGER_RTT_Write(unsigned BufferIndex,
                          const void * pBuffer,
                          unsigned NumBytes) {
  (void)BufferIndex;
  (void)pBuffer;
  return NumBytes;
}

unsigned SEGGER_RTT_WriteString(unsigned BufferIndex, const char * pText) {
  (void)BufferIndex;
  (void)pText;
  return 0u;
}

int RTT_vprintfFramed(unsigned BufferIndex,
                      const char * pPrefix,
                      const char * pFormat,
                      va_list * pParamList,
                      const char * pSuffix) {
  (void)BufferIndex;
  (void)pPrefix;
  (void)pFormat;
  (void)pParamList;
  (void)pSuffix;
  return -1;
}

int main(void) {
  ResultSink = RTT_LogI32("i32", INT32_MIN);
  ResultSink = RTT_LogU32("u32", UINT32_MAX);
  ResultSink = RTT_LogHex32("hex", UINT32_C(0x89ABCDEF));
  ResultSink = RTT_LogPointer("ptr", (const void *)(uintptr_t)UINT32_C(0x80000001));
  return ResultSink == 0;
}
