#include "SimpleBase64.h"

static const char b64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void SimpleBase64::encode(char *output, const uint8_t *input, size_t len) {
    size_t i = 0, j = 0;
    uint32_t triple;

    while (i < len) {
        // 取3个字节，若不足补0
        uint8_t octet_a = i < len ? input[i++] : 0;
        uint8_t octet_b = i < len ? input[i++] : 0;
        uint8_t octet_c = i < len ? input[i++] : 0;

        triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        // 输出4个Base64字符
        output[j++] = b64_table[(triple >> 18) & 0x3F];
        output[j++] = b64_table[(triple >> 12) & 0x3F];
        output[j++] = b64_table[(triple >> 6) & 0x3F];
        output[j++] = b64_table[triple & 0x3F];
    }

    // 根据实际输入长度，补齐'='
    int mod = len % 3;
    if (mod > 0) {
        output[j - 1] = '=';
        if (mod == 1) {
            output[j - 2] = '=';
        }
    }

    output[j] = '\0'; // 字符串结束符
}