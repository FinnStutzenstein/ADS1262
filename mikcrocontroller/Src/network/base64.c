/*
 * base64.c
 *
 *  Created on: Jan 16, 2019
 *      Author: finn
 */
#include "base64.h"

const char base64_characters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* A-Z, a-z, 0-9, +, /
 * not =, because this should just appear on the end.
 */
uint8_t is_base64_char(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
        || c == '+' || c == '/';
}

// Make the 4 chars of the three bytes at value[i]..value[i+2].
#define B64_A(value, i)         uint8_t a = (value[i] & 0xFC) >> 2;
#define B64_B(value, i)         uint8_t b = ((value[i] & 0x03) << 4) | ((value[i+1] & 0xF0) >> 4);
#define B64_C(value, i)         uint8_t c = ((value[i+1] & 0x0F) << 2) | ((value[i+2] & 0xC0) >> 6);
#define B64_D(value, i)         uint8_t d = value[i+2] & 0x3F;

/**
 * Makes the base64 of the value. Saved into out. len gives the length of the value.
 *
 * Not the best implementation, but is enough for the one usecase. E.g. the length of the written
 * string is not given or checked; but at websocket keys, we know how long the result will be.
 * ==> Use with caution!!
 */
void to_base64(unsigned char* value, char* out, int len) {
	// Process blocks of 3 bytes. They will be transfomed to 4 characters.
    int full_blocks = len/3;
    int remainder = len%3;
    int remainder_start = len - remainder;
    for (int i = 0; i < full_blocks; i++) {
        B64_A(value, i*3);
        B64_B(value, i*3);
        B64_C(value, i*3);
        B64_D(value, i*3);
        out[i*4] = base64_characters[a];
        out[i*4+1] = base64_characters[b];
        out[i*4+2] = base64_characters[c];
        out[i*4+3] = base64_characters[d];
    }
    // The remainder may be 1 or 2 bytes.
    if (remainder == 1) {
        B64_A(value, remainder_start);
        B64_B(value, remainder_start);
        out[full_blocks*4] = base64_characters[a];
        out[full_blocks*4+1] = base64_characters[b];
        out[full_blocks*4+2] = '=';
        out[full_blocks*4+3] = '=';
    } else if (remainder == 2) {
        B64_A(value, remainder_start);
        B64_B(value, remainder_start);
        uint8_t c = ((value[remainder_start+1] & 0x0F) << 2); // No second part.
        out[full_blocks*4] = base64_characters[a];
        out[full_blocks*4+1] = base64_characters[b];
        out[full_blocks*4+2] = base64_characters[c];
        out[full_blocks*4+3] = '=';
    }
}
