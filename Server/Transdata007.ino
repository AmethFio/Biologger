#include <Adafruit_TinyUSB.h>
#include <Arduino.h>
#include "SHA256.h"
#include "Cipher.h"

#include "Crypto.h"
#include "AES.h"
#include <ArduinoJson.h>
#include <time.h>
#include "SimpleBase64.h"
#include "HMAC_SHA256.h"

// heatshrink 头文件
#include "heatshrink_encoder.h"

#define DEVICE_ID "nrf52840"
#define HMAC_KEY "greatkey"

const uint8_t AES_KEY[16] = {
    's','e','c','r','e','t','k','e','y','1','2','3','4','5','6','7'
};
uint8_t iv[16];

#define MAX_BUFFER_SIZE (6000 * 3)  // 足够大
#define COMPRESSED_SIZE (6000 * 3) // 估计值
#define ENCRYPTED_SIZE (6000 * 6)
uint8_t binary[MAX_BUFFER_SIZE];
size_t binary_len = 0;

uint8_t compressedData[COMPRESSED_SIZE];
size_t compressedLen = 0;

uint8_t aesOutput[ENCRYPTED_SIZE];
size_t aesOutputLen = 0;

String payloadJson = "";  // JSON字符串
size_t payloadLen = 0;
unsigned long timestamp = 0;

String payloadStr;
String base64enc;
uint8_t hmacResult[32]; 

unsigned long start_timestamp = 1753082629UL;
int simulatedDataLen = 0;
const unsigned long DAY_SECONDS = 24UL * 60UL * 60UL;
const int activities[6] = {0, 1, 2, 3, 4, 5};
const float prior[5] = {0.2, 0.1, 0.4, 0.2, 0.1};
const int uniform_params[6][2] = {
    {0, 0}, {5, 25}, {10, 60}, {1, 15}, {10, 31}, {5, 95}
};
randomSeed(analogRead(A0));

// get timestamp
time_t getts(HardwareSerial& modem, unsigned long timeoutMs = 2000) {
    // 清空缓冲区
    while (modem.available()) modem.read();

    // 发送命令
    modem.println(F("AT+CCLK?"));

    String cclkLine;
    unsigned long start = millis();

    // 等待回应
    while (millis() - start < timeoutMs) {
        if (modem.available()) {
            String line = modem.readStringUntil('\n');
            line.trim();
            if (line.startsWith(F("+CCLK:"))) {
                cclkLine = line;
                break;
            }
        }
    }

    if (cclkLine.length() == 0) {
        Serial.println(F("[getts] ERROR: No CCLK response"));
        return 0;
    }

    // 提取引号里的部分
    int quote1 = cclkLine.indexOf('"');
    int quote2 = cclkLine.lastIndexOf('"');
    if (quote1 < 0 || quote2 <= quote1) {
        Serial.println(F("[getts] ERROR: Bad CCLK format"));
        return 0;
    }

    String ts = cclkLine.substring(quote1 + 1, quote2);
    Serial.print(F("[getts] CCLK: "));
    Serial.println(ts);

    // 解析时间字符串
    int year, month, day, hour, minute, second;
    if (sscanf(ts.c_str(), "%d/%d/%d,%d:%d:%d",
               &year, &month, &day, &hour, &minute, &second) != 6) {
        Serial.println(F("[getts] ERROR: Failed to parse timestamp"));
        return 0;
    }

    // 修正年份
    if (year < 100) year += 2000;

    // 构建 tm 结构体
    tm t {};
    t.tm_year = year - 1900;  // tm_year 从 1900 年开始
    t.tm_mon  = month - 1;    // tm_mon 从 0 开始
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min  = minute;
    t.tm_sec  = second;

    time_t unixTs = mktime(&t);
    return unixTs;
}

// 随机抽样活动
int sample_activity() {
    float r = random(0, 10000) / 10000.0; // 0~1
    float prob = 0.0;
    for (int i = 0; i < 5; i++) {
        prob += prior[i];
        if (r <= prob) {
            return i + 1;
        }
    }
    return 5;  // fallback
}

// 生成24小时数据 & 序列 & 二值化
void generateData(uint8_t* buffer, size_t buf_size) {
    binary_len = 0;
    unsigned long total_elapsed = 0;

    // 写入4字节起始时间戳
    start_timestamp = getts(Serial1);
    buffer[binary_len++] = (start_timestamp >> 24) & 0xFF;
    buffer[binary_len++] = (start_timestamp >> 16) & 0xFF;
    buffer[binary_len++] = (start_timestamp >> 8) & 0xFF;
    buffer[binary_len++] = (start_timestamp) & 0xFF;

    // 写第一个起始点
    buffer[binary_len++] = 0;  // delta_t high
    buffer[binary_len++] = 0;  // delta_t low
    buffer[binary_len++] = 0;  // a (-1)
    simulatedDataLen++;

    while (total_elapsed < DAY_SECONDS && binary_len + 3 <= buf_size) {
        int label = sample_activity();
        int min_val = uniform_params[label][0];
        int max_val = uniform_params[label][1];
        unsigned long delta = random(min_val, max_val + 1);

        // 写入3字节
        buffer[binary_len++] = (delta >> 8) & 0xFF;
        buffer[binary_len++] = delta & 0xFF;
        buffer[binary_len++] = activities[label];
        
        simulatedDataLen++;
        total_elapsed += delta;
    }

    Serial.print(F("Start timestamp: "));
    Serial.println(start_timestamp);
    Serial.println(F("First events: (delta_t, action)"));

    int idx = 4;
    int count = 0;
    while (idx + 3 <= binary_len && count < 5) {  // 打印前5个事件
        unsigned int delta_t =
            ((unsigned int)buffer[idx] << 8) |
            ((unsigned int)buffer[idx + 1]);
        uint8_t action = buffer[idx + 2];
    
        Serial.print(F("  Event "));
        Serial.print(count);
        Serial.print(F(": delta_t="));
        Serial.print(delta_t);
        Serial.print(F(", action="));
        Serial.println(action);
    
        idx += 3;
        count++;
    }
    Serial.println();
    Serial.print("Generated data len: ");
    Serial.println(binary_len);
    Serial.print("Generated entries: ");
    Serial.println(simulatedDataLen);

    // buffer = (uint8_t*)realloc(buffer, binary_len);  // Not recommend to realloc
}


// 🔷 compressData
void compressData(const uint8_t *buffer, size_t buffer_len) {
    heatshrink_encoder hse;
    heatshrink_encoder_reset(&hse);

    size_t sunk = 0, polled = 0;
    size_t totalOut = 0;

    while (sunk < buffer_len) {
        size_t this_sunk = 0;

        heatshrink_encoder_sink(&hse, (uint8_t*)&buffer[sunk], buffer_len - sunk, &this_sunk);
        sunk += this_sunk;

        int res;
        do {
            res = heatshrink_encoder_poll(&hse, &compressedData[totalOut], COMPRESSED_SIZE - totalOut, &polled);
            totalOut += polled;
        } while (res == HSER_POLL_MORE);
    }

    heatshrink_encoder_finish(&hse);

    int res;
    do {
        res = heatshrink_encoder_poll(&hse, &compressedData[totalOut], COMPRESSED_SIZE - totalOut, &polled);
        totalOut += polled;
    } while (res == HSER_POLL_MORE);

    compressedLen = totalOut;

    Serial.println("Compression finished. First 20 bytes:");
    for (int i = 0; i < 20 && i < compressedLen; i++) {
        if (compressedData[i] < 16) Serial.print('0');
        Serial.print(compressedData[i], HEX);
        Serial.print(' ');
    }
    Serial.println();
    Serial.print("Compressed data len: ");
    Serial.println(compressedLen);
}

void aes_encrypt_cbc(const uint8_t *in, size_t inLen) {
    AES128 aes;
    aes.setKey(AES_KEY, 16);

    // IV: 16 字节随机
    uint8_t iv[16];
    for (int i = 0; i < 16; i++) {
        iv[i] = random(0, 256);
    }

    Serial.println("IV generated!");

    memcpy(aesOutput, iv, 16);  // 先存 IV
    aesOutputLen = 16;

    // 填充 PKCS7
    size_t paddedLen = inLen;
    size_t padLen = 16 - (paddedLen % 16);
    paddedLen += padLen;

    uint8_t *padded = (uint8_t*)malloc(paddedLen);
    if (!padded) {
        Serial.println(F("AES: malloc failed!"));
        return;
    }

    memcpy(padded, in, inLen);
    for (size_t i = 0; i < padLen; i++) {
        padded[inLen + i] = padLen;
    }

    uint8_t prev[16];
    memcpy(prev, iv, 16);

    for (size_t offset = 0; offset < paddedLen; offset += 16) {
        // XOR with prev
        for (int i = 0; i < 16; i++) {
            padded[offset + i] ^= prev[i];
        }

        // Encrypt block
        aes.encryptBlock(&aesOutput[aesOutputLen], &padded[offset]);

        // Save this ciphertext block as prev
        memcpy(prev, &aesOutput[aesOutputLen], 16);

        aesOutputLen += 16;
    }

    free(padded);
    Serial.println(F("AES-CBC encryption done."));
    Serial.print(F("Encrypted length: "));
    Serial.println(aesOutputLen);
}

// 🔷 buildPayload
String buildPayload() {
    // 1. 获取timestamp
    timestamp = getts(Serial1);
    if (timestamp > 0) {
        Serial.print(F("Unix timestamp: "));
        Serial.println(timestamp);
    } else {
        Serial.println(F("Failed to get UNIX timestamp"));
        timestamp = millis() / 1000;
    }

    // 2. AES加密
    aes_encrypt_cbc(compressedData, compressedLen);

    // 构造 message = timestamp + device_id + compressedData
    String tsStr = String(timestamp);
    size_t messageLen = tsStr.length() + strlen(DEVICE_ID) + aesOutputLen;
    uint8_t *message = (uint8_t *)malloc(messageLen);
    if (!message) {
        Serial.println("Message allocation failed!");
        return "";
    }

    
    // 3. 填充 message
    size_t offset = 0;
    memcpy(message + offset, tsStr.c_str(), tsStr.length());
    offset += tsStr.length();
    memcpy(message + offset, DEVICE_ID, strlen(DEVICE_ID));
    offset += strlen(DEVICE_ID);
    memcpy(message + offset, aesOutput, aesOutputLen);

    Serial.print("messageLen: "); Serial.println(messageLen);
    for (size_t i = 0; i < 10 && i < messageLen; ++i) {
        Serial.print(message[i], HEX); Serial.print(" ");
    }
    Serial.println();

    // 4. 计算 HMAC
    HMAC_SHA256::compute(
        (const uint8_t *)HMAC_KEY, strlen(HMAC_KEY),
        message, messageLen,
        hmacResult
    );

    Serial.print("HMAC: ");
    for (int i = 0; i < 32; i++) {
        if (hmacResult[i] < 16) Serial.print("0");
        Serial.print(hmacResult[i], HEX);
    }
    Serial.println();

    free(message);

    // Base64 编码
    char hmacB64[64] = {0};  // 足够存储 base64(32字节)=44字节+终止符
    SimpleBase64::encode(hmacB64, (const uint8_t *)hmacResult, 32);

    // 手动补齐
    size_t len = strlen(hmacB64);
    if (len % 4 == 2) {
        strcat(hmacB64, "==");
    } else if (len % 4 == 3) {
        strcat(hmacB64, "=");
    }

    Serial.print("HMAC (Base64): ");
    Serial.println(hmacB64);

    // 5. Base64 编码
    size_t base64Len = (aesOutputLen + 2) / 3 * 4 + 1;
    char *base64Encoded = (char *)malloc(base64Len);
    if (!base64Encoded) {
        Serial.println("Base64 memory allocation failed!");
        return "";
    }
    memset(base64Encoded, 0, base64Len);
    SimpleBase64::encode(base64Encoded, aesOutput, aesOutputLen);


    // 6. 拼 JSON
    payloadJson = "{";
    payloadJson += "\"device_id\":\"";
    payloadJson += DEVICE_ID;
    payloadJson += "\",\"timestamp\":";
    payloadJson += timestamp;
    payloadJson += ",\"data\":\"";
    payloadJson += base64Encoded;
    payloadJson += "\",\"hmac\":\"";
    payloadJson += hmacB64;
    payloadJson += "\"}";

    payloadLen = payloadJson.length();

    Serial.println("Raw JSON payload:");
    Serial.println(payloadJson);

    free(base64Encoded);

    return payloadJson;

}

// 🔷 transmit
void transmit() {
    if (payloadJson.length() == 0) {
        Serial.println("[transmit] Payload is empty. Nothing to send.");
        return;
    }

    Serial.println("Transmitting payload and HMAC to nRF9160…");
    Serial.print("Payload length: "); Serial.println(payloadLen);

    // 1️⃣ 发送 AT#XHTTPCREQ 进入 datamode
    Serial1.print("AT#XHTTPCREQ=\"POST\",\"/data\",\"\",\"application/json\",");
    Serial1.println(payloadLen);

    bool ok = false;
    unsigned long start = millis();

    // 等待 OK 和 datamode 提示
    while (millis() - start < 3000) {
        if (Serial1.available()) {
            String line = Serial1.readStringUntil('\n');
            line.trim();
            Serial.print("[nRF9160] ");
            Serial.println(line);
            if (line.indexOf("OK") >= 0) {
                ok = true;
            }
            if (line.indexOf("Enter datamode") >= 0) {
                ok = true;
                break;
            }
            if (line.startsWith("ERROR")) {
                Serial.println("[transmit] AT command error.");
                return;
            }
        }
    }

    if (!ok) {
        Serial.println("[transmit] ERROR: Did not enter datamode.");
        return;
    }

    Serial.println("[transmit] Datamode ready. Sending payload…");

    // 2️⃣ 发送 JSON payload
    Serial1.print(payloadJson);

    delay(50); // 确保数据发出

    // 3️⃣ 发送 0x1A 表示结束
    Serial1.write(0x1A);

    Serial.println("[transmit] Payload + EOF sent. Waiting for response…");

    // 4️⃣ 读取服务器响应
    start = millis();
    while (millis() - start < 5000) {
        if (Serial1.available()) {
            String line = Serial1.readStringUntil('\n');
            line.trim();
            if (line.length() > 0) {
                Serial.print("[nRF9160] ");
                Serial.println(line);
            }
        }
    }

    Serial.println("Transmit done.");
}

// 🔷 AT Commands
void sendAT(String cmd, unsigned long waitTime = 500) {
    Serial.print("[Forwarded to nrf9160] ");
    Serial.println(cmd);
    Serial1.println(cmd);

    delay(50);  // 稍微等模块启动输出

    // unsigned long start = millis();
    // while (millis() - start < waitTime) {
    //     if (Serial1.available()) {
    //         char c = Serial1.read();
    //         Serial.write(c);  // 打印模块返回内容
    //         start = millis(); // 重置超时，只要有新数据就继续等
    //     }
    // }
}

// 🔷 setup
void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    Serial.println("Ready.");
    Serial.print("Serial1 RX: "); Serial.println(PIN_SERIAL1_RX);
    Serial.print("Serial1 TX: "); Serial.println(PIN_SERIAL1_TX);
    Serial1.begin(115200);  // nRF9160
    Serial.println("Serial bridge ready.");
}

// 🔷 loop
void loop() {
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();

        if (cmd == "generate") {
            generateData(binary, MAX_BUFFER_SIZE);
        } else if (cmd == "compress") {
            compressData(binary, binary_len);
        } else if (cmd == "payload") {
            buildPayload();
        } else if (cmd == "transmit") {
            transmit();
        } else {
            sendAT(cmd, 1000);
        }
    }

    // 实时打印从 nRF9160 返回的内容
    while (Serial1.available()) {
        char c = Serial1.read();
        Serial.write(c);
    }
}