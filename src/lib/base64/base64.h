#ifndef STUDENT_INFO_MANAGER_BASE64_H
#define STUDENT_INFO_MANAGER_BASE64_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/* The caller owns the returned Base64 string. */
static inline char *encodeBase64(const char *data, size_t length)
{
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const unsigned char *bytes = (const unsigned char *)data;
    size_t blocks = length / 3 + (length % 3 != 0);
    size_t encodedLength;
    char *encoded;

    if (blocks > (SIZE_MAX - 1) / 4) {
        return NULL;
    }
    encodedLength = blocks * 4;
    encoded = malloc(encodedLength + 1);
    if (encoded == NULL) {
        return NULL;
    }
    for (size_t block = 0; block < blocks; ++block) {
        size_t offset = block * 3;
        char *output = encoded + block * 4;
        size_t remaining = length - offset;
        unsigned int first = bytes[offset];
        unsigned int second = remaining > 1 ? bytes[offset + 1] : 0;
        unsigned int third = remaining > 2 ? bytes[offset + 2] : 0;

        output[0] = alphabet[first >> 2];
        output[1] = alphabet[((first & 3) << 4) | (second >> 4)];
        output[2] = remaining > 1 ? alphabet[((second & 15) << 2) | (third >> 6)] : '=';
        output[3] = remaining > 2 ? alphabet[third & 63] : '=';
    }
    encoded[encodedLength] = '\0';
    return encoded;
}

static inline int base64Value(unsigned char character)
{
    if (character >= 'A' && character <= 'Z') {
        return character - 'A';
    }
    if (character >= 'a' && character <= 'z') {
        return character - 'a' + 26;
    }
    if (character >= '0' && character <= '9') {
        return character - '0' + 52;
    }
    if (character == '+') {
        return 62;
    }
    if (character == '/') {
        return 63;
    }
    return -1;
}

/* Decodes in place. The caller must own the buffer and its terminator space. */
static inline int decodeBase64(char *buffer, size_t *length)
{
    unsigned char *bytes = (unsigned char *)buffer;
    size_t written = 0;

    if (*length == 0 || *length % 4 != 0) {
        return 0;
    }

    for (size_t offset = 0; offset < *length; offset += 4) {
        int firstValue = base64Value(bytes[offset]);
        int secondValue = base64Value(bytes[offset + 1]);
        int thirdValue = base64Value(bytes[offset + 2]);
        int fourthValue = base64Value(bytes[offset + 3]);

        if (firstValue < 0 || secondValue < 0) {
            return 0;
        }
        if (thirdValue < 0 || fourthValue < 0) {
            if (offset + 4 != *length || bytes[offset + 3] != '=') {
                return 0;
            }
            if (thirdValue < 0 && (bytes[offset + 2] != '=' || (secondValue & 15) != 0)) {
                return 0;
            }
            if (thirdValue >= 0 && (thirdValue & 3) != 0) {
                return 0;
            }
        }

        bytes[written++] = (unsigned char)((firstValue << 2) | (secondValue >> 4));
        if (thirdValue >= 0) {
            bytes[written++] = (unsigned char)(((secondValue & 15) << 4) | (thirdValue >> 2));
        }
        if (fourthValue >= 0) {
            bytes[written++] = (unsigned char)(((thirdValue & 3) << 6) | fourthValue);
        }
    }

    bytes[written] = '\0';
    *length = written;
    return 1;
}

#endif
