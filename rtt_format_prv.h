#ifndef RTT_FORMAT_PRV_H
#define RTT_FORMAT_PRV_H

#include <stdarg.h>

/*
 * 内部统一格式化入口：前缀、正文和后缀共享同一个缓冲及错误状态，保证
 * 日志装饰和正文按顺序连续写出。pParamList 由调用方负责初始化和释放。
 * 成功时返回实际写入字符数，RTT 短写或计数溢出时返回 -1。
 */
int RTT_vprintfFramed(unsigned BufferIndex,
                      const char * pPrefix,
                      const char * pFormat,
                      va_list * pParamList,
                      const char * pSuffix);

#endif
