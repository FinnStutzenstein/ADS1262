/*
 * utils.c
 *
 *  Created on: Oct 3, 2018
 *      Author: finn
 */

#include "utils.h"
#include "stdio.h"
#include "stm32f7xx_hal.h"
#include "string.h"

/**
 * Prints the data wiht the given length to the ITM debugger.
 */
void print_to_debugger(char* data, int len) {
	for (int i = 0; i < len; i++) {
		ITM_SendChar(data[i]);
	}
}

/**
 * Prints the string to the ITM debugger.
 */
void print_to_debugger_str(char* str) {
	print_to_debugger(str, strlen(str));
}

/**
 * Compares two strings caseinsensitive
 */
int strcicmp(char const *a, char const *b) {
    for (;; a++, b++) {
        int diff = tolower((unsigned char)*a) - tolower((unsigned char)*b);
        if (diff != 0 || !*a) {
            return diff;
        }
    }
}

/**
 * Checks, if find is a part of s ignoring the case.
 */
int is_strcistr(const char *s, const char *find) {
	size_t s_len = strlen(s);
	size_t find_len = strlen(find);
	if (find_len > s_len) {
		return 0;
	}
	size_t max = s_len - find_len;
	uint8_t ok;
	for (size_t i = 0; i <= max; i++) {
		ok = 1;
		for (size_t j = 0; j < find_len; j++) {
			if (tolower(s[i+j]) != tolower((unsigned char)find[j])) {
				ok = 0;
				break;
			}
		}
		if (ok) {
			return 1;
		}
	}
	return 0;
}

// Usefull for debugging. Prints the data in hex in blocks of 8.
static void __used print_hex(uint8_t* data, uint16_t len) {
	if (len == 0) {
		printf("no data\n");
		return;
	}
	if (len > 64) {
		printf("len is too big (%u)\n", len);
		len = 64;
	}

	// blocks of 8
	uint8_t blocksize = 8;
	uint8_t blocks = len / blocksize;
	uint8_t remainder = len % blocksize;

	for (int block = 0; block < blocks; block++) {
		uint16_t idx = block * blocksize;
		printf("%x %x %x %x %x %x %x %x\n", data[idx+0], data[idx+1], data[idx+2], data[idx+3], data[idx+4], data[idx+5], data[idx+6], data[idx+7]);
	}
	for (int r = 0; r < remainder; r++) {
		uint16_t idx = (len-remainder) + r;
		printf("%x ", data[idx]);
	}
	if (remainder > 0) {
		printf("\n");
	}
}
