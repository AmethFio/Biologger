#ifndef SIMPLE_BASE64_H
#define SIMPLE_BASE64_H

#include <Arduino.h>

class SimpleBase64 {
public:
    /**
     * 将 input 编码为 Base64，输出到 output。
     * output 需要至少 ((len+2)/3)*4 +1 字节空间
     */
    static void encode(char *output, const uint8_t *input, size_t len);
};

#endif