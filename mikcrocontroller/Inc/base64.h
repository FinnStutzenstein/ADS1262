/*
 * base64.h
 *
 *  Created on: Jan 16, 2019
 *      Author: finn
 */

#ifndef BASE64_H_
#define BASE64_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

uint8_t is_base64_char(char c);
void to_base64(unsigned char* value, char* out, int len);

#ifdef __cplusplus
}
#endif

#endif /* BASE64_H_ */
