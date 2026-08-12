#include "rtt_float.h"
#include "rtt_log.h"
#include "SEGGER_RTT.h"

#include <stddef.h>
#include <stdint.h>

#if RTT_LOG_ENABLE && LOG_ENABLE_FLOAT && HARD_FPU_ENABLE
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

static void rtt_log_float_parts(const char * sDescription,
                                unsigned Negative,
                                uint32_t IntegerPart,
                                unsigned DecimalPart) {
  const char * sSign;
  volatile int Timeout;

  sSign = Negative ? "-" : "";
  Timeout = 100;
  while (Timeout-- > 0) {
    int Result;

    if (sDescription != NULL) {
      Result = SEGGER_RTT_printf(RTT_LOG_BUFFER_INDEX, "%s: %s%u.%03u\n",
                                 sDescription, sSign, IntegerPart, DecimalPart);
    } else {
      Result = SEGGER_RTT_printf(RTT_LOG_BUFFER_INDEX, "%s%u.%03u\n",
                                 sSign, IntegerPart, DecimalPart);
    }
    if (Result >= 0) {
      break;
    }
  }
}

#if HARD_FPU_ENABLE

void RTT_LogFloat3(float Value, const char * sDescription) {
  float IntegerPart;
  float Fraction;
  float Magnitude;
  uint32_t IntegerPartAbs;
  unsigned DecimalPart;
  unsigned Negative;

  if (Value != Value) {
    rtt_log_float_text(sDescription, "NaN");
    return;
  }
  if ((Value == Value) && ((Value - Value) != (Value - Value))) {
    rtt_log_float_text(sDescription, Value < 0.0f ? "-Inf" : "Inf");
    return;
  }

  Negative = Value < 0.0f;
  Magnitude = Negative ? -Value : Value;
  if (Magnitude >= 0x1p32f) {
    rtt_log_float_text(sDescription, Negative ? "-Overflow" : "Overflow");
    return;
  }

  IntegerPart = 0.0f;
  Fraction = modff(Magnitude, &IntegerPart);
  IntegerPartAbs = (uint32_t)IntegerPart;
  DecimalPart = (unsigned)(Fraction * 1000.0f);

  rtt_log_float_parts(sDescription, Negative, IntegerPartAbs, DecimalPart);
}

#else

static void rtt_float_to_parts(float Value,
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
  uint32_t FractionBits;
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
      ScaledFraction = (Mantissa << 7) - (Mantissa << 1) - Mantissa;
      if (FractionShift >= 35u) {
        *pDecimalPart = 0u;
      } else {
        *pDecimalPart = (unsigned)(ScaledFraction >> (FractionShift - 3u));
      }
      return;
    }
    *pIntegerPart = Mantissa >> FractionShift;
    FractionBits = Mantissa & ((1u << FractionShift) - 1u);
    /* FractionBits * 125 fits in 32 bits and avoids divide helpers on Cortex-M0. */
    ScaledFraction = (FractionBits << 7) - (FractionBits << 1) - FractionBits;
    if (FractionShift >= 3u) {
      *pDecimalPart = (unsigned)(ScaledFraction >> (FractionShift - 3u));
    } else {
      *pDecimalPart = (unsigned)(ScaledFraction << (3u - FractionShift));
    }
  }
}

void RTT_LogFloat3(float Value, const char * sDescription) {
  uint32_t IntegerPart;
  unsigned DecimalPart;
  unsigned Negative;
  unsigned Special;

  rtt_float_to_parts(Value, &IntegerPart, &DecimalPart, &Negative, &Special);
  if (Special == RTT_FLOAT_SPECIAL_NAN) {
    rtt_log_float_text(sDescription, "NaN");
  } else if (Special == RTT_FLOAT_SPECIAL_INFINITY) {
    rtt_log_float_text(sDescription, Negative ? "-Inf" : "Inf");
  } else if (Special == RTT_FLOAT_SPECIAL_OVERFLOW) {
    rtt_log_float_text(sDescription, Negative ? "-Overflow" : "Overflow");
  } else {
    if ((IntegerPart == 0u) && (DecimalPart == 0u)) {
      Negative = 0u;
    }
    rtt_log_float_parts(sDescription, Negative, IntegerPart, DecimalPart);
  }
}

#endif

#else

void RTT_LogFloat3(float Value, const char * sDescription) {
  (void)Value;
  (void)sDescription;
}

#endif
