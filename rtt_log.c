#include "rtt_log.h"
#include "rtt_format_prv.h"

#include <stdarg.h>

#if RTT_LOG_ENABLE

/*
 * 返回一条日志的帧头。轻量模式没有帧头；PRINT 是原样打印接口，也不带
 * 级别标记。函数只返回静态字符串，不产生额外缓冲区或动态内存开销。
 */
static const char * rtt_log_prefix(RTT_LOG_LEVEL Level) {
#if LOG_ENABLE_LITE
  (void)Level;
  return "";
#elif RTT_LOG_USE_COLOR
  switch (Level) {
  case RTT_LOG_LEVEL_INFO:
    return RTT_CTRL_TEXT_BRIGHT_GREEN "[INFO] ";
  case RTT_LOG_LEVEL_DEBUG:
    return RTT_CTRL_TEXT_BRIGHT_BLUE "[DEBUG] ";
  case RTT_LOG_LEVEL_WARN:
    return RTT_CTRL_TEXT_BRIGHT_YELLOW "[WARN] ";
  case RTT_LOG_LEVEL_ERROR:
    return RTT_CTRL_TEXT_BRIGHT_RED "[ERROR] ";
  default:
    return "";
  }
#else
  switch (Level) {
  case RTT_LOG_LEVEL_INFO:
    return "[INFO] ";
  case RTT_LOG_LEVEL_DEBUG:
    return "[DEBUG] ";
  case RTT_LOG_LEVEL_WARN:
    return "[WARN] ";
  case RTT_LOG_LEVEL_ERROR:
    return "[ERROR] ";
  default:
    return "";
  }
#endif
}

/*
 * 所有日志均以换行结束。彩色级别日志还需先复位终端属性，避免颜色影响
 * 后续 RTT 输出；PRINT 未设置颜色，因此无需发送复位序列。
 */
static const char * rtt_log_suffix(RTT_LOG_LEVEL Level) {
#if LOG_ENABLE_LITE
  (void)Level;
#elif RTT_LOG_USE_COLOR
  if (Level != RTT_LOG_LEVEL_PRINT) {
    return RTT_CTRL_RESET "\n";
  }
#else
  (void)Level;
#endif
  return "\n";
}

int RTT_LogPrintf(RTT_LOG_LEVEL Level, const char * pFormat, ...) {
  int Result;
  va_list ParamList;

  va_start(ParamList, pFormat);
  /* 前缀、正文和后缀由同一次格式化过程连续写入，保持一条日志的帧结构。 */
  Result = RTT_vprintfFramed(RTT_LOG_BUFFER_INDEX,
                             rtt_log_prefix(Level),
                             pFormat,
                             &ParamList,
                             rtt_log_suffix(Level));
  va_end(ParamList);
  return Result;
}

int RTT_LogString(const char * pText) {
#if LOG_ENABLE_STRING
  unsigned Length;

  if (pText == NULL) {
    return -1;
  }
  Length = SEGGER_RTT_WriteString(RTT_LOG_BUFFER_INDEX, pText);
  return (int)Length;
#else
  (void)pText;
  return 0;
#endif
}

#if RTT_LOG_ENABLE && LOG_ENABLE_TYPED

#define RTT_TYPED_LABEL_MAX_SIZE    46u
#define RTT_TYPED_MIN_FRAME_SIZE    64u
#define RTT_TYPED_I32_VALUE_SIZE    12u
#define RTT_TYPED_U32_VALUE_SIZE    11u
#define RTT_TYPED_HEX32_VALUE_SIZE  11u
#define RTT_TYPED_POINTER_SIZE      ((sizeof(uintptr_t) * 2u) + 3u)
#define RTT_TYPED_POINTER_FRAME_SIZE \
  (RTT_TYPED_LABEL_MAX_SIZE + 2u + RTT_TYPED_POINTER_SIZE)
#define RTT_TYPED_LOG_FRAME_SIZE \
  ((RTT_TYPED_POINTER_FRAME_SIZE > RTT_TYPED_MIN_FRAME_SIZE) \
     ? RTT_TYPED_POINTER_FRAME_SIZE : RTT_TYPED_MIN_FRAME_SIZE)
#define RTT_TYPED_LOG_ERROR         (-1)

static uint32_t rtt_typed_divide_u32_by_10(uint32_t Value,
                                            unsigned * pRemainder) {
  uint32_t Quotient;

  Quotient = (Value >> 1) + (Value >> 2);
  Quotient += Quotient >> 4;
  Quotient += Quotient >> 8;
  Quotient += Quotient >> 16;
  Quotient >>= 3;
  Quotient += ((Value - (Quotient * 10u)) + 6u) >> 4;
  *pRemainder = (unsigned)(Value - (Quotient * 10u));
  return Quotient;
}

static unsigned rtt_typed_build_u32_digits(char * pBuffer, uint32_t Value) {
  unsigned End = 0u;
  unsigned Left;
  unsigned Right;

  do {
    unsigned Digit;

    Value = rtt_typed_divide_u32_by_10(Value, &Digit);
    pBuffer[End++] = (char)('0' + Digit);
  } while (Value != 0u);

  Left = 0u;
  Right = End - 1u;
  while (Left < Right) {
    char Temporary = pBuffer[Left];

    pBuffer[Left++] = pBuffer[Right];
    pBuffer[Right--] = Temporary;
  }
  return End;
}

static unsigned rtt_typed_build_i32(char * pBuffer, int32_t Value) {
  uint32_t Magnitude;
  unsigned Length = 0u;

  if (Value < 0) {
    pBuffer[Length++] = '-';
    Magnitude = 0u - (uint32_t)Value;
  } else {
    Magnitude = (uint32_t)Value;
  }
  Length += rtt_typed_build_u32_digits(pBuffer + Length, Magnitude);
  pBuffer[Length++] = '\n';
  return Length;
}

static unsigned rtt_typed_build_u32(char * pBuffer, uint32_t Value) {
  unsigned Length = rtt_typed_build_u32_digits(pBuffer, Value);

  pBuffer[Length++] = '\n';
  return Length;
}

static unsigned rtt_typed_build_hex(char * pBuffer,
                                    uintptr_t Value,
                                    unsigned NumDigits) {
  static const char HexDigits[] = "0123456789ABCDEF";
  unsigned Index;

  pBuffer[0] = '0';
  pBuffer[1] = 'x';
  for (Index = NumDigits; Index != 0u; --Index) {
    pBuffer[Index + 1u] = HexDigits[Value & 0x0Fu];
    Value >>= 4;
  }
  pBuffer[NumDigits + 2u] = '\n';
  return NumDigits + 3u;
}

static int rtt_typed_write_frame(const char * pLabel,
                                 const char * pValue,
                                 unsigned ValueLength) {
  char Frame[RTT_TYPED_LOG_FRAME_SIZE];
  unsigned LabelLength = 0u;
  unsigned TotalLength;
  unsigned Index;

  if ((pLabel != NULL) && (*pLabel != '\0')) {
    while ((LabelLength < RTT_TYPED_LABEL_MAX_SIZE) &&
           (pLabel[LabelLength] != '\0')) {
      ++LabelLength;
    }
  }

  TotalLength = LabelLength + ValueLength;
  if (LabelLength != 0u) {
    TotalLength += 2u;
  }

  Index = 0u;
  if (LabelLength != 0u) {
    unsigned LabelIndex;

    for (LabelIndex = 0u; LabelIndex < LabelLength; ++LabelIndex) {
      Frame[Index++] = pLabel[LabelIndex];
    }
    Frame[Index++] = ':';
    Frame[Index++] = ' ';
  }
  while (Index < TotalLength) {
    Frame[Index] = pValue[Index - LabelLength -
                          ((LabelLength != 0u) ? 2u : 0u)];
    ++Index;
  }
  if (SEGGER_RTT_Write(RTT_LOG_BUFFER_INDEX, Frame, TotalLength) != TotalLength) {
    return RTT_TYPED_LOG_ERROR;
  }
  return (int)TotalLength;
}

int RTT_LogI32(const char * pLabel, int32_t Value) {
  char ValueText[RTT_TYPED_I32_VALUE_SIZE];
  unsigned ValueLength = rtt_typed_build_i32(ValueText, Value);

  return rtt_typed_write_frame(pLabel, ValueText, ValueLength);
}

int RTT_LogU32(const char * pLabel, uint32_t Value) {
  char ValueText[RTT_TYPED_U32_VALUE_SIZE];
  unsigned ValueLength = rtt_typed_build_u32(ValueText, Value);

  return rtt_typed_write_frame(pLabel, ValueText, ValueLength);
}

int RTT_LogHex32(const char * pLabel, uint32_t Value) {
  char ValueText[RTT_TYPED_HEX32_VALUE_SIZE];
  unsigned ValueLength = rtt_typed_build_hex(ValueText, (uintptr_t)Value, 8u);

  return rtt_typed_write_frame(pLabel, ValueText, ValueLength);
}

int RTT_LogPointer(const char * pLabel, const void * pValue) {
  char ValueText[RTT_TYPED_POINTER_SIZE];
  unsigned ValueLength =
    rtt_typed_build_hex(ValueText,
                        (uintptr_t)pValue,
                        (unsigned)(sizeof(uintptr_t) * 2u));

  return rtt_typed_write_frame(pLabel, ValueText, ValueLength);
}

#endif

#else

/*
 * 保留禁用配置下的函数符号，兼容直接调用 RTT_LogPrintf 的代码；正常使用
 * log_* 宏时调用会在预处理阶段被完全移除。
 */
int RTT_LogPrintf(RTT_LOG_LEVEL Level, const char * pFormat, ...) {
  (void)Level;
  (void)pFormat;
  return 0;
}

int RTT_LogString(const char * pText) {
  (void)pText;
  return 0;
}

#endif
