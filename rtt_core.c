#include "rtt_core.h"
#include "rtt_log.h"
#include "SEGGER_RTT.h"

#include <stddef.h>
#include <stdint.h>

#if RTT_LOG_ENABLE && LOG_ENABLE_FLOAT && HAS_FPU
  #include <math.h>
#endif

#define RTT_FLOAT_SPECIAL_NONE     (0u)
#define RTT_FLOAT_SPECIAL_NAN      (1u)
#define RTT_FLOAT_SPECIAL_INFINITY (2u)
#define RTT_FLOAT_SPECIAL_OVERFLOW (3u)

#if RTT_LOG_ENABLE && LOG_ENABLE_FLOAT

static void rtt_log_float_text(const char * sDescription, const char * sText) {
  volatile int Timeout;

  Timeout = 100;
  while (Timeout-- > 0) {
    int Result;

    if (sDescription != NULL) {
      Result = SEGGER_RTT_printf(RTT_LOG_BUFFER_INDEX, "%s: %s\n", sDescription, sText);
    } else {
      Result = SEGGER_RTT_printf(RTT_LOG_BUFFER_INDEX, "%s\n", sText);
    }
    if (Result >= 0) {
      break;
    }
  }
}

#if HAS_FPU

void RTT_LogFloat3(float Value, const char * sDescription) {
  float IntegerPart;
  float Fraction;
  float Magnitude;
  uint32_t IntegerPartAbs;
  unsigned DecimalPart;
  unsigned Negative;
  const char * sSign;
  volatile int Timeout;

  if (Value != Value) {
    rtt_log_float_text(sDescription, "NaN");
    return;
  }
  if ((Value == Value) && ((Value - Value) != (Value - Value))) {
    rtt_log_float_text(sDescription, Value < 0.0f ? "-Inf" : "Inf");
    return;
  }

  Negative = Value < 0.0f;
  sSign = Negative ? "-" : "";
  Magnitude = Negative ? -Value : Value;
  if (Magnitude >= 0x1p32f) {
    rtt_log_float_text(sDescription, Negative ? "-Overflow" : "Overflow");
    return;
  }

  IntegerPart = 0.0f;
  Fraction = modff(Magnitude, &IntegerPart);
  IntegerPartAbs = (uint32_t)IntegerPart;
  DecimalPart = (unsigned)(Fraction * 1000.0f);

  Timeout = 100;
  while (Timeout-- > 0) {
    int Result;

    if (sDescription != NULL) {
      Result = SEGGER_RTT_printf(RTT_LOG_BUFFER_INDEX, "%s: %s%u.%03u\n",
                                 sDescription, sSign, IntegerPartAbs, DecimalPart);
    } else {
      Result = SEGGER_RTT_printf(RTT_LOG_BUFFER_INDEX, "%s%u.%03u\n",
                                 sSign, IntegerPartAbs, DecimalPart);
    }
    if (Result >= 0) {
      break;
    }
  }
}

#else

static void rtt_float_to_milli(float Value,
                               uint32_t * pMilli,
                               unsigned * pNegative,
                               unsigned * pSpecial) {
  union {
    float f;
    uint32_t u;
  } Bits;
  uint32_t Exponent;
  uint32_t Mantissa;
  uint32_t Milli;
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
    *pMilli = 0u;
    return;
  }

  Mantissa |= 0x800000u;
  /* Mantissa * 125 fits in 32 bits and avoids soft-float helpers on Cortex-M0. */
  Milli = (Mantissa << 7) - (Mantissa << 1) - Mantissa;
  Shift = (int)Exponent - 147;
  if (Shift >= 0) {
    if ((Shift >= 32) || (Milli > (UINT32_MAX >> Shift))) {
      *pSpecial = RTT_FLOAT_SPECIAL_OVERFLOW;
      return;
    }
    Milli <<= Shift;
  } else if (Shift <= -32) {
    Milli = 0u;
  } else {
    Milli >>= (unsigned)-Shift;
  }
  *pMilli = Milli;
}

static void rtt_milli_to_text(uint32_t Milli, unsigned Negative, char * pText) {
  static const uint32_t Places[] = {
    1000000000u, 100000000u, 10000000u, 1000000u, 100000u,
    10000u, 1000u, 100u, 10u, 1u
  };
  unsigned Digit;
  unsigned Index;
  unsigned Started;

  if ((Negative != 0u) && (Milli != 0u)) {
    *pText++ = '-';
  }
  Started = 0u;
  for (Index = 0u; Index < 10u; Index++) {
    Digit = 0u;
    while (Milli >= Places[Index]) {
      Milli -= Places[Index];
      Digit++;
    }
    if ((Started != 0u) || (Digit != 0u) || (Index == 6u)) {
      *pText++ = (char)('0' + Digit);
      Started = 1u;
    }
    if (Index == 6u) {
      *pText++ = '.';
    }
  }
  *pText = '\0';
}

void RTT_LogFloat3(float Value, const char * sDescription) {
  uint32_t Milli;
  unsigned Negative;
  unsigned Special;
  char Text[14];

  rtt_float_to_milli(Value, &Milli, &Negative, &Special);
  if (Special == RTT_FLOAT_SPECIAL_NAN) {
    rtt_log_float_text(sDescription, "NaN");
  } else if (Special == RTT_FLOAT_SPECIAL_INFINITY) {
    rtt_log_float_text(sDescription, Negative ? "-Inf" : "Inf");
  } else if (Special == RTT_FLOAT_SPECIAL_OVERFLOW) {
    rtt_log_float_text(sDescription, Negative ? "-Overflow" : "Overflow");
  } else {
    rtt_milli_to_text(Milli, Negative, Text);
    rtt_log_float_text(sDescription, Text);
  }
}

#endif

#else

void RTT_LogFloat3(float Value, const char * sDescription) {
  (void)Value;
  (void)sDescription;
}

#endif
