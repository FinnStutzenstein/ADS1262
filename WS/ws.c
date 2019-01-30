#include <stdio.h>
#include <string.h>
#include "sha1.h"
#include <stdint.h>
#include <ctype.h>

#define WEBSOCKET_KEY_LENGTH    24
#define WEBSOCKET_ACCEPT_LENGTH 28
#define WEBSOCKET_MAGIC         "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define WEBSOCKET_MAGIC_LENGTH  38

const char base64_characters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int strcimatch(const char *s, const char *find) {
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

/* A-Z, a-z, 0-9, +, /
 * not =, because this should just appear on the end.
 */
int is_base64_char(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
        || c == '+' || c == '/';
}

int is_valid_key(const char* key) {
    if (strlen(key) != WEBSOCKET_KEY_LENGTH || key[WEBSOCKET_KEY_LENGTH-1] != '='
        || key[WEBSOCKET_KEY_LENGTH-2] != '=') {
        return 0;
    }
    for (int i = 0; i < 22; i++) {
        if (!is_base64_char(key[i])) {
            return 0;
        }
    }
    return 1;
}

#define B64_A(value, i)         uint8_t a = (value[i] & 0xFC) >> 2;
#define B64_B(value, i)         uint8_t b = ((value[i] & 0x03) << 4) | ((value[i+1] & 0xF0) >> 4);
#define B64_C(value, i)         uint8_t c = ((value[i+1] & 0x0F) << 2) | ((value[i+2] & 0xC0) >> 6);
#define B64_D(value, i)         uint8_t d = value[i+2] & 0x3F;
void to_base64(unsigned char* value, char* out, int len) {
    int full_blocks = len/3;
    int remainder = len%3;
    int remainder_start = len - remainder;
    for (int i = 0; i < full_blocks; i++) {
        B64_A(value, i*3);
        B64_B(value, i*3);
        B64_C(value, i*3);
        B64_D(value, i*3);
        if (a >= 64 || b >= 64 || c >= 64 || d >= 64) {
            printf("ERROR\n");
            return;
        }
        out[i*4] = base64_characters[a];
        out[i*4+1] = base64_characters[b];
        out[i*4+2] = base64_characters[c];
        out[i*4+3] = base64_characters[d];
    }
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
        // B64_C(value, remainder_start);
        uint8_t c = ((value[remainder_start+1] & 0x0F) << 2);
        printf("a: %u, b: %u, c: %u\n", a, b, c);
        out[full_blocks*4] = base64_characters[a];
        out[full_blocks*4+1] = base64_characters[b];
        out[full_blocks*4+2] = base64_characters[c];
        out[full_blocks*4+3] = '=';
    }
}

int main(int argc, char** argv) {
    // The data to be hashed
    const char key[] = "bhAe5LVdrInTKRkqQ6KgUA==";
    //const char correct_accept[] = "mFl4jRjjFZCZ3e2PYcwaHfiDc4x=";
    if (!is_valid_key(key)) {
        return 1;
    }
    char buffer[WEBSOCKET_KEY_LENGTH+WEBSOCKET_MAGIC_LENGTH + 1];
    strcpy(buffer, key);
    strcpy(buffer+WEBSOCKET_KEY_LENGTH, WEBSOCKET_MAGIC);
    printf("%s\n", buffer);

    size_t length = strlen(buffer);
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1((const unsigned char*)buffer, length, hash);

    char accept[WEBSOCKET_ACCEPT_LENGTH + 1];
    to_base64(hash, accept, SHA_DIGEST_LENGTH);
    printf("%s\n", accept);
    //printf("%s\n", correct_accept);
}
