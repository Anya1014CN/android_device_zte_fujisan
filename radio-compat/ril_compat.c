/*
 * Compatibility implementation of the legacy libcutils strdup8to16 ABI.
 * Qualcomm's Oreo RIL imports this C symbol, which is absent on Android 12.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static uint32_t decode_utf8(const unsigned char **input)
{
    const unsigned char *s = *input;
    uint32_t codepoint;
    unsigned int continuation_count;

    if (s[0] < 0x80) {
        *input = s + 1;
        return s[0];
    }

    if ((s[0] & 0xe0) == 0xc0) {
        codepoint = s[0] & 0x1f;
        continuation_count = 1;
    } else if ((s[0] & 0xf0) == 0xe0) {
        codepoint = s[0] & 0x0f;
        continuation_count = 2;
    } else if ((s[0] & 0xf8) == 0xf0) {
        codepoint = s[0] & 0x07;
        continuation_count = 3;
    } else {
        *input = s + 1;
        return 0xfffd;
    }

    for (unsigned int i = 0; i < continuation_count; ++i) {
        const unsigned char next = s[i + 1];
        if (next == '\0' || (next & 0xc0) != 0x80) {
            *input = s + 1;
            return 0xfffd;
        }
        codepoint = (codepoint << 6) | (next & 0x3f);
    }

    *input = s + continuation_count + 1;
    return codepoint;
}

uint16_t *strdup8to16(const char *source, size_t *out_length)
{
    if (source == NULL) {
        return NULL;
    }

    const unsigned char *input = (const unsigned char *)source;
    size_t output_length = 0;
    while (*input != '\0') {
        const uint32_t codepoint = decode_utf8(&input);
        output_length += codepoint > 0xffff ? 2 : 1;
    }

    uint16_t *output = calloc(output_length + 1, sizeof(*output));
    if (output == NULL) {
        return NULL;
    }

    input = (const unsigned char *)source;
    size_t index = 0;
    while (*input != '\0') {
        uint32_t codepoint = decode_utf8(&input);
        if (codepoint > 0x10ffff || (codepoint >= 0xd800 && codepoint <= 0xdfff)) {
            codepoint = 0xfffd;
        }
        if (codepoint > 0xffff) {
            codepoint -= 0x10000;
            output[index++] = 0xd800 | (codepoint >> 10);
            output[index++] = 0xdc00 | (codepoint & 0x3ff);
        } else {
            output[index++] = (uint16_t)codepoint;
        }
    }
    if (out_length != NULL) {
        *out_length = output_length;
    }
    return output;
}
