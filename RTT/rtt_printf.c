/*********************************************************************
*                    SEGGER Microcontroller GmbH                     *
*                        The Embedded Experts                        *
**********************************************************************
*                                                                    *
*            (c) 1995 - 2021 SEGGER Microcontroller GmbH             *
*                                                                    *
*       www.segger.com     Support: support@segger.com               *
*                                                                    *
**********************************************************************
*                                                                    *
*       SEGGER RTT * Real Time Transfer for embedded targets         *
*                                                                    *
**********************************************************************
*                                                                    *
* All rights reserved.                                               *
*                                                                    *
* Redistribution and use in source and binary forms, with or         *
* without modification, are permitted provided that the following    *
* condition is met:                                                  *
*                                                                    *
* o Redistributions of source code must retain the above copyright   *
*   notice, this condition and the following disclaimer.             *
*                                                                    *
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND             *
* CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,        *
* INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF           *
* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE           *
* DISCLAIMED. IN NO EVENT SHALL SEGGER Microcontroller BE LIABLE FOR *
* ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR           *
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT  *
* OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;    *
* OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF      *
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT          *
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE  *
* USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH   *
* DAMAGE.                                                            *
*                                                                    *
**********************************************************************
*/

#include "SEGGER_RTT.h"
#include "SEGGER_RTT_Conf.h"

#include <limits.h>
#include <stdarg.h>
#include <string.h>

#ifndef SEGGER_RTT_PRINTF_BUFFER_SIZE
  #define SEGGER_RTT_PRINTF_BUFFER_SIZE (64u)
#endif

#if (SEGGER_RTT_PRINTF_BUFFER_SIZE < 1)
  #error "SEGGER_RTT_PRINTF_BUFFER_SIZE must be at least 1"
#endif

#define FORMAT_FLAG_LEFT_JUSTIFY (1u << 0)
#define FORMAT_FLAG_PAD_ZERO     (1u << 1)
#define FORMAT_FLAG_PRINT_SIGN   (1u << 2)

typedef struct {
  unsigned BufferIndex;
  unsigned Count;
  unsigned Used;
  int      Error;
  char     Buffer[SEGGER_RTT_PRINTF_BUFFER_SIZE];
} RTT_PRINTF_DESC;

typedef struct {
  unsigned Flags;
  unsigned FieldWidth;
  unsigned Precision;
} RTT_FORMAT_DESC;

#if defined(__GNUC__) || defined(__clang__)
  #define RTT_PRINTF_ALWAYS_INLINE __attribute__((always_inline)) inline
#else
  #define RTT_PRINTF_ALWAYS_INLINE inline
#endif

static RTT_PRINTF_ALWAYS_INLINE void _Flush(RTT_PRINTF_DESC * pDesc) {
  unsigned Used;

  Used = pDesc->Used;
  if (Used == 0u) {
    return;
  }
  if (SEGGER_RTT_Write(pDesc->BufferIndex, pDesc->Buffer, Used) != Used) {
    pDesc->Error = -1;
    return;
  }
  pDesc->Used = 0u;
}

static void _Store(RTT_PRINTF_DESC * pDesc,
                   const char * pData,
                   char Fill,
                   unsigned Length) {
  while ((Length != 0u) && (pDesc->Error == 0)) {
    unsigned Avail;
    unsigned Chunk;

    Avail = SEGGER_RTT_PRINTF_BUFFER_SIZE - pDesc->Used;
    Chunk = (Length < Avail) ? Length : Avail;
    if (pData != NULL) {
      memcpy(pDesc->Buffer + pDesc->Used, pData, Chunk);
      pData += Chunk;
    } else {
      memset(pDesc->Buffer + pDesc->Used, Fill, Chunk);
    }
    pDesc->Used += Chunk;
    pDesc->Count += Chunk;
    Length -= Chunk;
    if (pDesc->Used == SEGGER_RTT_PRINTF_BUFFER_SIZE) {
      _Flush(pDesc);
    }
  }
}

static void _StoreSpan(RTT_PRINTF_DESC * pDesc, const char * pData, unsigned Length) {
  _Store(pDesc, pData, '\0', Length);
}

static void _StoreRepeat(RTT_PRINTF_DESC * pDesc, char c, unsigned Count) {
  _Store(pDesc, NULL, c, Count);
}

static void _StoreChar(RTT_PRINTF_DESC * pDesc, char c) {
  _Store(pDesc, &c, '\0', 1u);
}

static unsigned _ParseUnsigned(const char ** ps) {
  const char * s;
  unsigned Value;

  s = *ps;
  Value = 0u;
  while ((*s >= '0') && (*s <= '9')) {
    unsigned Digit;

    Digit = (unsigned)(*s - '0');
    Value = (Value * 10u) + Digit;
    s++;
  }
  *ps = s;
  return Value;
}

static uintptr_t _DivideBy10(uintptr_t Value, unsigned * pRemainder) {
#if defined(__ARM_ARCH_6M__) || defined(__ARM_ARCH_8M_BASE__)
  uintptr_t Quotient;

  // Exact 32-bit division by 10 without pulling in the Armv6-M divide helper.
  Quotient = (Value >> 1) + (Value >> 2);
  Quotient += Quotient >> 4;
  Quotient += Quotient >> 8;
  Quotient += Quotient >> 16;
  Quotient >>= 3;
  Quotient += ((Value - (Quotient * 10u)) + 6u) >> 4;
  *pRemainder = (unsigned)(Value - (Quotient * 10u));
  return Quotient;
#else
  uintptr_t Quotient;

  Quotient = Value / 10u;
  *pRemainder = (unsigned)(Value - (Quotient * 10u));
  return Quotient;
#endif
}

static char * _BuildDecimal(char * pEnd, uintptr_t Value) {
  do {
    unsigned Digit;

    Value = _DivideBy10(Value, &Digit);
    *--pEnd = (char)('0' + Digit);
  } while (Value != 0u);
  return pEnd;
}

static char * _BuildHex(char * pEnd, uintptr_t Value) {
  do {
    unsigned Digit;

    Digit = (unsigned)(Value & 0x0Fu);
    *--pEnd = (char)((Digit < 10u) ? ('0' + Digit) : ('A' + Digit - 10u));
    Value >>= 4;
  } while (Value != 0u);
  return pEnd;
}

static void _PrintNumber(RTT_PRINTF_DESC * pDesc,
                         uintptr_t Value,
                         unsigned Base,
                         char Sign,
                         const RTT_FORMAT_DESC * pFormat) {
  char Digits[(sizeof(uintptr_t) * 2u) + 2u];
  char * pEnd;
  char * pDigits;
  unsigned NumDigits;
  unsigned NumZeros;
  unsigned ContentWidth;
  unsigned NumPadding;
  unsigned FormatFlags;
  unsigned Precision;

  pEnd = Digits + sizeof(Digits);
  if (Base == 16u) {
    pDigits = _BuildHex(pEnd, Value);
  } else {
    pDigits = _BuildDecimal(pEnd, Value);
  }
  NumDigits = (unsigned)(pEnd - pDigits);
  FormatFlags = pFormat->Flags;
  Precision = pFormat->Precision;
  if (Precision == UINT_MAX) {
    Precision = 0u;
  } else {
    FormatFlags &= ~FORMAT_FLAG_PAD_ZERO;
  }
  NumZeros = (Precision > NumDigits) ? (Precision - NumDigits) : 0u;
  ContentWidth = NumDigits + NumZeros + ((Sign != '\0') ? 1u : 0u);
  NumPadding = (pFormat->FieldWidth > ContentWidth) ?
               (pFormat->FieldWidth - ContentWidth) : 0u;

  if ((FormatFlags & FORMAT_FLAG_LEFT_JUSTIFY) != 0u) {
    FormatFlags &= ~FORMAT_FLAG_PAD_ZERO;
  }
  if (((FormatFlags & FORMAT_FLAG_LEFT_JUSTIFY) == 0u) &&
      ((FormatFlags & FORMAT_FLAG_PAD_ZERO) == 0u)) {
    _StoreRepeat(pDesc, ' ', NumPadding);
  }
  if (Sign != '\0') {
    _StoreChar(pDesc, Sign);
  }
  if (((FormatFlags & FORMAT_FLAG_LEFT_JUSTIFY) == 0u) &&
      ((FormatFlags & FORMAT_FLAG_PAD_ZERO) != 0u)) {
    _StoreRepeat(pDesc, '0', NumPadding);
  }
  _StoreRepeat(pDesc, '0', NumZeros);
  _StoreSpan(pDesc, pDigits, NumDigits);
  if ((FormatFlags & FORMAT_FLAG_LEFT_JUSTIFY) != 0u) {
    _StoreRepeat(pDesc, ' ', NumPadding);
  }
}

static void _PrintString(RTT_PRINTF_DESC * pDesc, const char * s, unsigned Precision) {
  unsigned Length;

  Length = 0u;
  while ((s[Length] != '\0') &&
         ((Precision == UINT_MAX) || (Length < Precision))) {
    Length++;
  }
  _StoreSpan(pDesc, s, Length);
}

int SEGGER_RTT_vprintf(unsigned BufferIndex, const char * sFormat, va_list * pParamList) {
  RTT_PRINTF_DESC Desc;

  Desc.BufferIndex = BufferIndex;
  Desc.Count = 0u;
  Desc.Used = 0u;
  Desc.Error = 0;

  while ((*sFormat != '\0') && (Desc.Error == 0)) {
    const char * pLiteral;
    const char * pConversion;
    RTT_FORMAT_DESC Format;
    char Specifier;

    pLiteral = sFormat;
    while ((*sFormat != '\0') && (*sFormat != '%')) {
      sFormat++;
    }
    _StoreSpan(&Desc, pLiteral, (unsigned)(sFormat - pLiteral));
    if ((*sFormat == '\0') || (Desc.Error != 0)) {
      break;
    }

    pConversion = sFormat++;
    Format.Flags = 0u;
    for (;;) {
      if (*sFormat == '-') {
        Format.Flags |= FORMAT_FLAG_LEFT_JUSTIFY;
      } else if (*sFormat == '0') {
        Format.Flags |= FORMAT_FLAG_PAD_ZERO;
      } else if (*sFormat == '+') {
        Format.Flags |= FORMAT_FLAG_PRINT_SIGN;
      } else {
        break;
      }
      sFormat++;
    }

    Format.FieldWidth = _ParseUnsigned(&sFormat);
    Format.Precision = UINT_MAX;
    if (*sFormat == '.') {
      int DynamicPrecision;

      sFormat++;
      Format.Precision = 0u;
      if (*sFormat == '*') {
        sFormat++;
        DynamicPrecision = va_arg(*pParamList, int);
        if (DynamicPrecision < 0) {
          Format.Precision = UINT_MAX;
        } else {
          Format.Precision = (unsigned)DynamicPrecision;
        }
      } else {
        Format.Precision = _ParseUnsigned(&sFormat);
      }
    }

    Specifier = *sFormat;
    if (Specifier == '\0') {
      _StoreSpan(&Desc, pConversion, (unsigned)(sFormat - pConversion));
      break;
    }
    sFormat++;

    switch (Specifier) {
    case 'c': {
      char Value;

      Value = (char)va_arg(*pParamList, int);
      _StoreChar(&Desc, Value);
      break;
    }
    case 'd': {
      int Value;
      unsigned Magnitude;
      char Sign;

      Value = va_arg(*pParamList, int);
      if (Value < 0) {
        Magnitude = 0u - (unsigned)Value;
        Sign = '-';
      } else {
        Magnitude = (unsigned)Value;
        Sign = ((Format.Flags & FORMAT_FLAG_PRINT_SIGN) != 0u) ? '+' : '\0';
      }
      _PrintNumber(&Desc, (uintptr_t)Magnitude, 10u, Sign, &Format);
      break;
    }
    case 'u':
      Format.Flags &= ~FORMAT_FLAG_PRINT_SIGN;
      _PrintNumber(&Desc, (uintptr_t)va_arg(*pParamList, unsigned int),
                   10u, '\0', &Format);
      break;
    case 'x':
    case 'X':
      Format.Flags &= ~FORMAT_FLAG_PRINT_SIGN;
      _PrintNumber(&Desc, (uintptr_t)va_arg(*pParamList, unsigned int),
                   16u, '\0', &Format);
      break;
    case 's': {
      const char * s;

      s = va_arg(*pParamList, const char *);
      if (s == NULL) {
        s = "(NULL)";
        Format.Precision = UINT_MAX;
      }
      _PrintString(&Desc, s, Format.Precision);
      break;
    }
    case 'p': {
      uintptr_t Value;
      unsigned PointerDigits;

      Value = (uintptr_t)va_arg(*pParamList, void *);
      PointerDigits = (unsigned)(sizeof(uintptr_t) * 2u);
      Format.Flags = 0u;
      Format.FieldWidth = PointerDigits;
      Format.Precision = PointerDigits;
      _PrintNumber(&Desc, Value, 16u, '\0', &Format);
      break;
    }
    case '%':
      _StoreChar(&Desc, '%');
      break;
    default:
      _StoreSpan(&Desc, pConversion, (unsigned)(sFormat - pConversion));
      break;
    }
  }

  if (Desc.Error == 0) {
    _Flush(&Desc);
  }
  if (Desc.Error == 0) {
    if (Desc.Count <= (unsigned)INT_MAX) {
      return (int)Desc.Count;
    }
  }
  return -1;
}

int SEGGER_RTT_printf(unsigned BufferIndex, const char * sFormat, ...) {
  int Result;
  va_list ParamList;

  va_start(ParamList, sFormat);
  Result = SEGGER_RTT_vprintf(BufferIndex, sFormat, &ParamList);
  va_end(ParamList);
  return Result;
}

/*************************** End of file ****************************/
