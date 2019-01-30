/*
 * utils.h
 *
 *  Created on: Sep 21, 2018
 *      Author: finn
 */

#ifndef __UTILS_H_
#define __UTILS_H_

#include "stdint.h"
#include "ctype.h"

#define BYTE_TO_BINARY_PATTERN "0b%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0')

#ifdef __cplusplus
 extern "C" {
#endif

void print_to_debugger(char* data, int len);
void print_to_debugger_str(char* str);
int strcicmp(char const *a, char const *b);
int is_strcistr(char const *a, char const *b);

#ifdef __cplusplus
}
#endif

#endif
