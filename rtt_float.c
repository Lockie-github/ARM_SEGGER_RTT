#include "rtt_float.h"
#include "rtt_log.h"
#include "SEGGER_RTT.h"

#include <stddef.h>
#include <stdint.h>

#if RTT_LOG_ENABLE && (LOG_ENABLE_FLOAT || LOG_ENABLE_TYPED_FLOAT) && \
    RTT_FLOAT_USE_MODFF
  #include <math.h>
#endif

#define RTT_FLOAT_SPECIAL_NONE     (0u)
#define RTT_FLOAT_SPECIAL_NAN      (1u)
#define RTT_FLOAT_SPECIAL_INFINITY (2u)
#define RTT_FLOAT_SPECIAL_OVERFLOW (3u)

#if RTT_LOG_ENABLE && (LOG_ENABLE_FLOAT || LOG_ENABLE_TYPED_FLOAT)

#if LOG_ENABLE_FLOAT
static void _LogFloatText(const char * sDescription, const char * sText) {
  if (sDescription != NULL) {
    (void)SEGGER_RTT_printf(RTT_LOG_BUFFER_INDEX, "%s: %s\n", sDescription, sText);
  } else {
    (void)SEGGER_RTT_printf(RTT_LOG_BUFFER_INDEX, "%s\n", sText);
  }
}
#endif

#if (LOG_ENABLE_FLOAT && RTT_LOG_FLOAT_FAST_PATH) || LOG_ENABLE_TYPED_FLOAT

#define RTT_FLOAT_DIRECT_BUFFER_SIZE (64u)

#if !defined(__ARM_FEATURE_IDIV)

/*
 * Divide by ten without using / or %.  The packed return value maps to the
 * two-register uint64_t return ABI on Arm: quotient in the high word and
 * remainder in the low word.  This avoids Cortex-M0 division helpers without
 * adding output-parameter stack slots to the float logging path.
 */
static uint64_t _DivMod10(uint32_t Value) {
  uint32_t Quotient;
  uint32_t Remainder;

  Quotient = (Value >> 1) + (Value >> 2);
  Quotient += Quotient >> 4;
  Quotient += Quotient >> 8;
  Quotient += Quotient >> 16;
  Quotient >>= 3;
  Remainder = Value - (((Quotient << 2) + Quotient) << 1);
  if (Remainder > 9u) {
    Remainder -= 10u;
    Quotient++;
  }
  return ((uint64_t)Quotient << 32) | Remainder;
}

#endif

#if LOG_ENABLE_TYPED_FLOAT

static char * _AppendTypedFloatParts(char * pCurrent,
                                     unsigned Negative,
                                     uint32_t IntegerPart,
                                     unsigned DecimalPart) {
  char * pDigits;
  char * pLeft;
  char * pRight;
#if !defined(__ARM_FEATURE_IDIV)
  uint64_t DivResult;
  unsigned DecimalDigit0;
  unsigned DecimalDigit1;
#endif

  if (Negative != 0u) {
    *pCurrent++ = '-';
  }

  pDigits = pCurrent;
  do {
#if defined(__ARM_FEATURE_IDIV)
    *pCurrent++ = (char)('0' + (IntegerPart % 10u));
    IntegerPart /= 10u;
#else
    DivResult = _DivMod10(IntegerPart);
    IntegerPart = (uint32_t)(DivResult >> 32);
    *pCurrent++ = (char)('0' + (unsigned)DivResult);
#endif
  } while (IntegerPart != 0u);
  pLeft = pDigits;
  pRight = pCurrent - 1;
  while (pLeft < pRight) {
    char Temp;

    Temp = *pLeft;
    *pLeft++ = *pRight;
    *pRight-- = Temp;
  }
  *pCurrent++ = '.';
#if defined(__ARM_FEATURE_IDIV)
  *pCurrent++ = (char)('0' + (DecimalPart / 100u));
  *pCurrent++ = (char)('0' + ((DecimalPart / 10u) % 10u));
  *pCurrent++ = (char)('0' + (DecimalPart % 10u));
#else
  DivResult = _DivMod10(DecimalPart);
  DecimalPart = (unsigned)(DivResult >> 32);
  DecimalDigit0 = (unsigned)DivResult;
  DivResult = _DivMod10(DecimalPart);
  DecimalPart = (unsigned)(DivResult >> 32);
  DecimalDigit1 = (unsigned)DivResult;
  *pCurrent++ = (char)('0' + DecimalPart);
  *pCurrent++ = (char)('0' + DecimalDigit1);
  *pCurrent++ = (char)('0' + DecimalDigit0);
#endif
  *pCurrent++ = '\n';
  return pCurrent;
}

#endif

#endif

#if LOG_ENABLE_FLOAT

#if RTT_LOG_FLOAT_FAST_PATH

/* 有限值完整帧能够放入 64 B 缓冲区时绕过 formatter。 */
static unsigned _TryLogFloatPartsDirect(const char * sDescription,
                                        unsigned Negative,
                                        uint32_t IntegerPart,
                                        unsigned DecimalPart) {
  char Buffer[RTT_FLOAT_DIRECT_BUFFER_SIZE];
  char * pCurrent = Buffer;
  char * pDigits;
  char * pLeft;
  char * pRight;
#if !defined(__ARM_FEATURE_IDIV)
  uint64_t DivResult;
  unsigned DecimalDigit0;
  unsigned DecimalDigit1;
#endif

  if (sDescription != NULL) {
    while (*sDescription != '\0') {
      if ((unsigned)(pCurrent - Buffer) >=
          (RTT_FLOAT_DIRECT_BUFFER_SIZE - 17u)) {
        return 0u;
      }
      *pCurrent++ = *sDescription++;
    }
    *pCurrent++ = ':';
    *pCurrent++ = ' ';
  }
  if (Negative != 0u) {
    *pCurrent++ = '-';
  }
  pDigits = pCurrent;
  do {
#if defined(__ARM_FEATURE_IDIV)
    *pCurrent++ = (char)('0' + (IntegerPart % 10u));
    IntegerPart /= 10u;
#else
    DivResult = _DivMod10(IntegerPart);
    IntegerPart = (uint32_t)(DivResult >> 32);
    *pCurrent++ = (char)('0' + (unsigned)DivResult);
#endif
  } while (IntegerPart != 0u);
  pLeft = pDigits;
  pRight = pCurrent - 1;
  while (pLeft < pRight) {
    char Temp = *pLeft;

    *pLeft++ = *pRight;
    *pRight-- = Temp;
  }
  *pCurrent++ = '.';
#if defined(__ARM_FEATURE_IDIV)
  *pCurrent++ = (char)('0' + (DecimalPart / 100u));
  *pCurrent++ = (char)('0' + ((DecimalPart / 10u) % 10u));
  *pCurrent++ = (char)('0' + (DecimalPart % 10u));
#else
  DivResult = _DivMod10(DecimalPart);
  DecimalPart = (unsigned)(DivResult >> 32);
  DecimalDigit0 = (unsigned)DivResult;
  DivResult = _DivMod10(DecimalPart);
  DecimalPart = (unsigned)(DivResult >> 32);
  DecimalDigit1 = (unsigned)DivResult;
  *pCurrent++ = (char)('0' + DecimalPart);
  *pCurrent++ = (char)('0' + DecimalDigit1);
  *pCurrent++ = (char)('0' + DecimalDigit0);
#endif
  *pCurrent++ = '\n';
  (void)SEGGER_RTT_Write(RTT_LOG_BUFFER_INDEX,
                         Buffer,
                         (unsigned)(pCurrent - Buffer));
  return 1u;
}

#endif

static void _LogFloatParts(const char * sDescription,
                           unsigned Negative,
                           uint32_t IntegerPart,
                           unsigned DecimalPart) {
  const char * sSign;

#if RTT_LOG_FLOAT_FAST_PATH
  if (_TryLogFloatPartsDirect(sDescription, Negative, IntegerPart, DecimalPart) != 0u) {
    return;
  }
#endif
  sSign = Negative ? "-" : "";
  if (sDescription != NULL) {
    (void)SEGGER_RTT_printf(RTT_LOG_BUFFER_INDEX, "%s: %s%u.%03u\n",
                            sDescription, sSign, IntegerPart, DecimalPart);
  } else {
    (void)SEGGER_RTT_printf(RTT_LOG_BUFFER_INDEX, "%s%u.%03u\n",
                            sSign, IntegerPart, DecimalPart);
  }
}

#endif

#if RTT_FLOAT_USE_MODFF

#if LOG_ENABLE_TYPED_FLOAT

/* 备用路径使用 modff，代码更通用，但会依赖目标的浮点运行库。 */
static void _FloatToParts(float Value,
                          uint32_t * pIntegerPart,
                          unsigned * pDecimalPart,
                          unsigned * pNegative,
                          unsigned * pSpecial) {
  float IntegerPart;
  float Fraction;
  float Magnitude;

  *pSpecial = RTT_FLOAT_SPECIAL_NONE;
  *pNegative = Value < 0.0f;
  /* IEEE-754 中只有 NaN 不等于自身。 */
  if (Value != Value) {
    *pSpecial = RTT_FLOAT_SPECIAL_NAN;
    return;
  }
  /* 有限数相减为 0；无穷减自身产生 NaN，由此避免额外的 isinf 依赖。 */
  if ((Value == Value) && ((Value - Value) != (Value - Value))) {
    *pSpecial = RTT_FLOAT_SPECIAL_INFINITY;
    return;
  }

  Magnitude = (*pNegative != 0u) ? -Value : Value;
  if (Magnitude >= 0x1p32f) {
    *pSpecial = RTT_FLOAT_SPECIAL_OVERFLOW;
    return;
  }

  IntegerPart = 0.0f;
  Fraction = modff(Magnitude, &IntegerPart);
  *pIntegerPart = (uint32_t)IntegerPart;
  /* 固定三位小数采用截断语义，与无 FPU 路径保持一致。 */
  *pDecimalPart = (unsigned)(Fraction * 1000.0f);
}

#endif

#if LOG_ENABLE_FLOAT

/* 保持 legacy modff 路径原有的控制流、栈布局和生成代码。 */
void RTT_LogFloat3(float Value, const char * sDescription) {
  float IntegerPart;
  float Fraction;
  float Magnitude;
  uint32_t IntegerPartAbs;
  unsigned DecimalPart;
  unsigned Negative;

  if (Value != Value) {
    _LogFloatText(sDescription, "NaN");
    return;
  }
  if ((Value == Value) && ((Value - Value) != (Value - Value))) {
    _LogFloatText(sDescription, Value < 0.0f ? "-Inf" : "Inf");
    return;
  }

  Negative = Value < 0.0f;
  Magnitude = Negative ? -Value : Value;
  if (Magnitude >= 0x1p32f) {
    _LogFloatText(sDescription, Negative ? "-Overflow" : "Overflow");
    return;
  }

  IntegerPart = 0.0f;
  Fraction = modff(Magnitude, &IntegerPart);
  IntegerPartAbs = (uint32_t)IntegerPart;
  DecimalPart = (unsigned)(Fraction * 1000.0f);
  if ((IntegerPartAbs == 0u) && (DecimalPart == 0u)) {
    Negative = 0u;
  }
  _LogFloatParts(sDescription, Negative, IntegerPartAbs, DecimalPart);
}

#endif

#else

/*
 * 纯转换核心只拆分数值，不执行格式化或 RTT 写入。legacy 与 typed API 同时
 * 启用时共享这一份实现；仅有一个调用方时允许编译器内联，以免增加调用开销。
 * 按 IEEE-754 binary32 的符号位、指数和尾数拆出十进制整数及三位小数，
 * 避免链接 modff 和软浮点除法。pSpecial 单独返回特殊值类别。
 */
static void _FloatToParts(float Value,
                          uint32_t * pIntegerPart,
                          unsigned * pDecimalPart,
                          unsigned * pNegative,
                          unsigned * pSpecial) {
  union {
    float f;
    uint32_t u;
  } Bits;
  uint32_t Exponent;
  uint32_t Mantissa;
  int Shift;

  Bits.f = Value;
  *pNegative = Bits.u >> 31;
  Exponent = (Bits.u >> 23) & 0xFFu;
  Mantissa = Bits.u & 0x7FFFFFu;
  *pSpecial = RTT_FLOAT_SPECIAL_NONE;
  if (Exponent == 0xFFu) {
    *pSpecial = (Mantissa == 0u) ? RTT_FLOAT_SPECIAL_INFINITY : RTT_FLOAT_SPECIAL_NAN;
    return;
  }
  if (Exponent == 0u) {
    *pIntegerPart = 0u;
    *pDecimalPart = 0u;
    return;
  }

  /* 规格化数补回隐含的最高位：Value = Mantissa * 2^(Exponent - 150)。 */
  Mantissa |= 0x800000u;
  Shift = (int)Exponent - 150;
  if (Shift >= 0) {
    if ((Shift >= 32) || (Mantissa > (UINT32_MAX >> Shift))) {
      *pSpecial = RTT_FLOAT_SPECIAL_OVERFLOW;
      return;
    }
    *pIntegerPart = Mantissa << Shift;
    *pDecimalPart = 0u;
  } else {
    unsigned FractionShift;
    uint32_t ScaledFraction;

    FractionShift = (unsigned)-Shift;
    if (FractionShift >= 32u) {
      *pIntegerPart = 0u;
      /* Mantissa * 1000 / 2^n = Mantissa * 125 / 2^(n-3)。 */
      ScaledFraction = (Mantissa << 7) - (Mantissa << 1) - Mantissa;
      if (FractionShift >= 35u) {
        *pDecimalPart = 0u;
      } else {
        *pDecimalPart = (unsigned)(ScaledFraction >> (FractionShift - 3u));
      }
      return;
    }
    uint32_t FractionBits;
    *pIntegerPart = Mantissa >> FractionShift;
    FractionBits = Mantissa & ((1u << FractionShift) - 1u);
    /* FractionBits * 125 不溢出 uint32_t，同时避免 Cortex-M0 的除法辅助函数。 */
    ScaledFraction = (FractionBits << 7) - (FractionBits << 1) - FractionBits;
    if (FractionShift >= 3u) {
      *pDecimalPart = (unsigned)(ScaledFraction >> (FractionShift - 3u));
    } else {
      *pDecimalPart = (unsigned)(ScaledFraction << (3u - FractionShift));
    }
  }
}

#if LOG_ENABLE_FLOAT

void RTT_LogFloat3(float Value, const char * sDescription) {
  uint32_t IntegerPart;
  unsigned DecimalPart;
  unsigned Negative;
  unsigned Special;

  _FloatToParts(Value, &IntegerPart, &DecimalPart, &Negative, &Special);
  if (Special == RTT_FLOAT_SPECIAL_NAN) {
    _LogFloatText(sDescription, "NaN");
  } else if (Special == RTT_FLOAT_SPECIAL_INFINITY) {
    _LogFloatText(sDescription, Negative ? "-Inf" : "Inf");
  } else if (Special == RTT_FLOAT_SPECIAL_OVERFLOW) {
    _LogFloatText(sDescription, Negative ? "-Overflow" : "Overflow");
  } else {
    /* 输出时将 -0 和绝对值小于 0.001 的负数统一规范为 0.000。 */
    if ((IntegerPart == 0u) && (DecimalPart == 0u)) {
      Negative = 0u;
    }
    _LogFloatParts(sDescription, Negative, IntegerPart, DecimalPart);
  }
}

#endif

#endif

#if LOG_ENABLE_TYPED_FLOAT

#define RTT_TYPED_FLOAT_LABEL_MAX_SIZE (46u)
#define RTT_TYPED_FLOAT_VALUE_MAX_SIZE (16u)

#if (RTT_TYPED_FLOAT_LABEL_MAX_SIZE + 2u + \
     RTT_TYPED_FLOAT_VALUE_MAX_SIZE) > RTT_FLOAT_DIRECT_BUFFER_SIZE
  #error "RTT typed float frame exceeds its direct buffer"
#endif

static char * _AppendTypedLabel(char * pBuffer, const char * pLabel) {
  char * pCurrent = pBuffer;

  if (pLabel != NULL) {
    while (((unsigned)(pCurrent - pBuffer) < RTT_TYPED_FLOAT_LABEL_MAX_SIZE) &&
           (*pLabel != '\0')) {
      *pCurrent++ = *pLabel++;
    }
  }
  if (pCurrent != pBuffer) {
    *pCurrent++ = ':';
    *pCurrent++ = ' ';
  }
  return pCurrent;
}

static char * _AppendFloatText(char * pCurrent, const char * pText) {
  while (*pText != '\0') {
    *pCurrent++ = *pText++;
  }
  *pCurrent++ = '\n';
  return pCurrent;
}

int RTT_LogF32(const char * pLabel, float Value) {
  char Buffer[RTT_FLOAT_DIRECT_BUFFER_SIZE];
  char * pCurrent;
  const char * pSpecialText;
  uint32_t IntegerPart;
  unsigned DecimalPart;
  unsigned Negative;
  unsigned Special;
  unsigned Length;

  _FloatToParts(Value, &IntegerPart, &DecimalPart, &Negative, &Special);
  pCurrent = _AppendTypedLabel(Buffer, pLabel);
  if (Special != RTT_FLOAT_SPECIAL_NONE) {
    if (Special == RTT_FLOAT_SPECIAL_NAN) {
      pSpecialText = "NaN";
    } else if (Special == RTT_FLOAT_SPECIAL_INFINITY) {
      pSpecialText = (Negative != 0u) ? "-Inf" : "Inf";
    } else {
      pSpecialText = (Negative != 0u) ? "-Overflow" : "Overflow";
    }
    pCurrent = _AppendFloatText(pCurrent, pSpecialText);
  } else {
    if ((IntegerPart == 0u) && (DecimalPart == 0u)) {
      Negative = 0u;
    }
    pCurrent = _AppendTypedFloatParts(pCurrent,
                                      Negative,
                                      IntegerPart,
                                      DecimalPart);
  }

  Length = (unsigned)(pCurrent - Buffer);
  if (SEGGER_RTT_Write(RTT_LOG_BUFFER_INDEX, Buffer, Length) != Length) {
    return -1;
  }
  return (int)Length;
}

#endif

#endif

#if !RTT_LOG_ENABLE || !LOG_ENABLE_FLOAT

/* 模块关闭时保留符号，避免直接调用方因条件编译产生链接差异。 */
void RTT_LogFloat3(float Value, const char * sDescription) {
  (void)Value;
  (void)sDescription;
}

#endif
