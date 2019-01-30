#ifndef _SHA1_H
#define _SHA1_H

#include "stddef.h"

#define SHA_DIGEST_LENGTH 20
unsigned char *SHA1(const unsigned char *d, size_t n, unsigned char *md);

#endif
