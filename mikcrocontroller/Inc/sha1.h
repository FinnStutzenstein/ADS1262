/*
 * sha1.h
 *
 *  Created on: Jan 16, 2019
 *      Author: finn
 */

#ifndef SHA1_H_
#define SHA1_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stddef.h"

#define SHA_DIGEST_LENGTH 20
unsigned char *SHA1(const unsigned char *d, size_t n, unsigned char *md);

#ifdef __cplusplus
}
#endif

#endif /* SHA1_H_ */
