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
#include "rtt_format_prv.h"

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
#define FORMAT_FLAG_HEX          (1u << 3)
#define FORMAT_FLAG_NEGATIVE     (1u << 4)
#define RTT_PRINTF_ERROR_COUNT   (1u << ((sizeof(unsigned) * CHAR_BIT) - 1u))

/* 一次格式化过程的输出状态；Buffer 只暂存小块数据，不限制整条消息长度。 */
typedef struct {
  unsigned BufferIndex;
  unsigned Count;
  unsigned Used;
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

#if defined(__GNUC__) && !defined(__clang__)
  #define RTT_PRINTF_NO_JUMP_TABLE __attribute__((optimize("no-jump-tables")))
#else
  #define RTT_PRINTF_NO_JUMP_TABLE
#endif

static RTT_PRINTF_ALWAYS_INLINE void _Flush(RTT_PRINTF_DESC * pDesc) {
  unsigned Used;

  Used = pDesc->Used;
  if (Used == 0u) {
    return;
  }
  /* SEGGER_RTT_Write 短写即视为整次格式化失败，不重复已写出的字节。 */
  if (SEGGER_RTT_Write(pDesc->BufferIndex, pDesc->Buffer, Used) != Used) {
    pDesc->Count = RTT_PRINTF_ERROR_COUNT;
    return;
  }
  pDesc->Used = 0u;
}

static void _Store(RTT_PRINTF_DESC * pDesc,
                   const char * pData,
                   char Fill,
                   unsigned Length) {
  while ((Length != 0u) && (pDesc->Count < RTT_PRINTF_ERROR_COUNT)) {
    unsigned Avail;
    unsigned Chunk;

    /* 按剩余空间分块，使任意长度的字段都能通过固定大小缓冲区输出。 */
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

  /* 精确计算 32 位无符号数除以 10，避免 Armv6-M 引入除法辅助函数。 */
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
                         RTT_FORMAT_DESC * pFormat) {
#if UINTPTR_MAX > UINT_MAX
  char Digits[(sizeof(uintptr_t) * 2u) + 2u];
#endif
  char * pEnd;
  char * pDigits;
  unsigned NumDigits;
  unsigned NumZeros;
  unsigned ContentWidth;
  unsigned NumPadding;
  unsigned FormatFlags;
  unsigned Precision;
  unsigned FieldWidth;
  unsigned Base;
  char Sign;

  FormatFlags = pFormat->Flags;
  FieldWidth = pFormat->FieldWidth;
  Precision = pFormat->Precision;
#if UINTPTR_MAX > UINT_MAX
  pEnd = Digits + sizeof(Digits);
#else
  /* 32位目标最多需要10个十进制字符，复用已读取完毕的12B格式描述符。 */
  pEnd = (char *)(pFormat + 1);
#endif
  Base = ((FormatFlags & FORMAT_FLAG_HEX) != 0u) ? 16u : 10u;
  if ((FormatFlags & FORMAT_FLAG_NEGATIVE) != 0u) {
    Sign = '-';
  } else {
    Sign = ((FormatFlags & FORMAT_FLAG_PRINT_SIGN) != 0u) ? '+' : '\0';
  }
  if (Base == 16u) {
    pDigits = _BuildHex(pEnd, Value);
  } else {
    pDigits = _BuildDecimal(pEnd, Value);
  }
  NumDigits = (unsigned)(pEnd - pDigits);
  /* UINT_MAX 表示格式串没有指定精度；显式精度会覆盖 '0' 补齐标志。 */
  if (Precision == UINT_MAX) {
    Precision = 0u;
  } else {
    FormatFlags &= ~FORMAT_FLAG_PAD_ZERO;
  }
  NumZeros = (Precision > NumDigits) ? (Precision - NumDigits) : 0u;
  ContentWidth = NumDigits + NumZeros + ((Sign != '\0') ? 1u : 0u);
  NumPadding = (FieldWidth > ContentWidth) ? (FieldWidth - ContentWidth) : 0u;

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

static RTT_PRINTF_NO_JUMP_TABLE void _FormatBody(RTT_PRINTF_DESC * pDesc,
                                                 const char * sFormat,
                                                 va_list * pParamList) {
  while ((*sFormat != '\0') && (pDesc->Count < RTT_PRINTF_ERROR_COUNT)) {
    const char * pLiteral;
    const char * pConversion;
    RTT_FORMAT_DESC Format;
    char Specifier;

    /* 先批量复制普通文本，再解析紧随其后的单个转换说明。 */
    pLiteral = sFormat;
    while ((*sFormat != '\0') && (*sFormat != '%')) {
      sFormat++;
    }
    _StoreSpan(pDesc, pLiteral, (unsigned)(sFormat - pLiteral));
    if ((*sFormat == '\0') || (pDesc->Count >= RTT_PRINTF_ERROR_COUNT)) {
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
      _StoreSpan(pDesc, pConversion, (unsigned)(sFormat - pConversion));
      break;
    }
    sFormat++;

    /*
     * 为控制固件体积，仅实现 c/d/u/x/X/s/p/%。不支持的转换说明按原文
     * 输出，且不会取走可变参数，便于在 RTT 中直接发现格式串不兼容。
     */
    switch (Specifier) {
    case 'c': {
      char Value;

      Value = (char)va_arg(*pParamList, int);
      _StoreChar(pDesc, Value);
      break;
    }
    case 'd': {
      int Value;
      unsigned Magnitude;

      Value = va_arg(*pParamList, int);
      if (Value < 0) {
        /* 先转无符号再求补码绝对值，可安全处理 INT_MIN。 */
        Magnitude = 0u - (unsigned)Value;
        Format.Flags |= FORMAT_FLAG_NEGATIVE;
      } else {
        Magnitude = (unsigned)Value;
        Format.Flags &= ~FORMAT_FLAG_NEGATIVE;
      }
      _PrintNumber(pDesc, (uintptr_t)Magnitude, &Format);
      break;
    }
    case 'u':
      Format.Flags &= ~(FORMAT_FLAG_PRINT_SIGN | FORMAT_FLAG_NEGATIVE |
                        FORMAT_FLAG_HEX);
      _PrintNumber(pDesc, (uintptr_t)va_arg(*pParamList, unsigned int), &Format);
      break;
    case 'x':
    case 'X':
      Format.Flags &= ~(FORMAT_FLAG_PRINT_SIGN | FORMAT_FLAG_NEGATIVE);
      Format.Flags |= FORMAT_FLAG_HEX;
      _PrintNumber(pDesc, (uintptr_t)va_arg(*pParamList, unsigned int), &Format);
      break;
    case 's': {
      const char * s;

      s = va_arg(*pParamList, const char *);
      if (s == NULL) {
        s = "(NULL)";
        Format.Precision = UINT_MAX;
      }
      _PrintString(pDesc, s, Format.Precision);
      break;
    }
    case 'p': {
      uintptr_t Value;
      unsigned PointerDigits;

      Value = (uintptr_t)va_arg(*pParamList, void *);
      /* 指针固定输出为无 0x 前缀、按架构位宽补零的大写十六进制。 */
      PointerDigits = (unsigned)(sizeof(uintptr_t) * 2u);
      Format.Flags = FORMAT_FLAG_HEX;
      Format.FieldWidth = PointerDigits;
      Format.Precision = PointerDigits;
      _PrintNumber(pDesc, Value, &Format);
      break;
    }
    case '%':
      _StoreChar(pDesc, '%');
      break;
    default:
      _StoreSpan(pDesc, pConversion, (unsigned)(sFormat - pConversion));
      break;
    }
  }

}

static void _InitDesc(RTT_PRINTF_DESC * pDesc, unsigned BufferIndex) {
  pDesc->BufferIndex = BufferIndex;
  pDesc->Count = 0u;
  pDesc->Used = 0u;
}

static int _FinishDesc(RTT_PRINTF_DESC * pDesc) {
  if (pDesc->Count < RTT_PRINTF_ERROR_COUNT) {
    _Flush(pDesc);
  }
  if (pDesc->Count <= (unsigned)INT_MAX) {
    return (int)pDesc->Count;
  }
  return -1;
}

int RTT_vprintfFramed(unsigned BufferIndex,
                      const char * pPrefix,
                      const char * sFormat,
                      va_list * pParamList,
                      const char * pSuffix) {
  RTT_PRINTF_DESC Desc;

  _InitDesc(&Desc, BufferIndex);
  /* 三段内容共用 Desc，长消息会自动多次刷新，但总字符数连续累计。 */
  _PrintString(&Desc, pPrefix, UINT_MAX);
  _FormatBody(&Desc, sFormat, pParamList);
  /* 仅在正文完整时追加后缀，避免写失败后继续产生残缺日志。 */
  if (Desc.Count < RTT_PRINTF_ERROR_COUNT) {
    _PrintString(&Desc, pSuffix, UINT_MAX);
  }
  return _FinishDesc(&Desc);
}

static int _RTT_vprintfRaw(unsigned BufferIndex,
                           const char * sFormat,
                           va_list * pParamList) {
  RTT_PRINTF_DESC Desc;

  _InitDesc(&Desc, BufferIndex);
  _FormatBody(&Desc, sFormat, pParamList);
  return _FinishDesc(&Desc);
}

int SEGGER_RTT_vprintf(unsigned BufferIndex, const char * sFormat, va_list * pParamList) {
  return _RTT_vprintfRaw(BufferIndex, sFormat, pParamList);
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
