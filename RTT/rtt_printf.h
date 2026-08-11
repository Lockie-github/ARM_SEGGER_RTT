#ifndef RTT_PRINTF_INTERNAL_H
#define RTT_PRINTF_INTERNAL_H

#include <stdarg.h>

int RTT_vprintfFramed(unsigned BufferIndex,
                      const char * pPrefix,
                      const char * pFormat,
                      va_list * pParamList,
                      const char * pSuffix);

#endif
