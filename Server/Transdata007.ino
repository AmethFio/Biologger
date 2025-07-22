#include <Adafruit_TinyUSB.h>
#include <Arduino.h>
#include "SHA256.h"

#include "Crypto.h"
#include "AES.h"
#include <ArduinoJson.h>
#include <time.h>
#include "SimpleBase64.h"
#include "HMAC_SHA256.h"

// heatshrink 头文件
#include "heatshrink_encoder.h"

#define RAW_SIZE 1024
#define COMPRESSED_SIZE (RAW_SIZE) // 估计值
#define ENCRYPTED_SIZE 2048
#define AES_BLOCK_SIZE 16
#define AES_KEY "1234567890abcdef"  // 16字节
#define AES_IV  "abcdef1234567890"  // 16字节
#define DEVICE_ID "nrf52840"
#define HMAC_KEY "greatkey"

uint8_t rawData[RAW_SIZE];
uint8_t compressedData[COMPRESSED_SIZE];
uint8_t encryptedData[ENCRYPTED_SIZE];
size_t compressedLen = 0;

String payloadJson = "";  // JSON字符串
size_t payloadLen = 0;
unsigned long timestamp = 0;

String payloadStr;
String base64enc;
uint8_t hmacResult[32]; 

// 🔷 generateData
void generateData() {
    for (size_t i = 0; i < RAW_SIZE; i++) {
        rawData[i] = "ABCDEF"[i % 6];
    }
    Serial.println("Raw data generated.");
    for (int i = 0; i < 20; i++) {
        Serial.print((char)rawData[i]);
    }
    Serial.println();
    Serial.print("Raw data len: "); Serial.println(strlen((char *)rawData));
}

// 🔷 compressData
void compressData() {
    heatshrink_encoder hse;
    heatshrink_encoder_reset(&hse);

    size_t sunk = 0, polled = 0;
    size_t totalOut = 0;

    size_t inputLen = strlen((char *)rawData);

    while (sunk < inputLen) {
        size_t this_sunk = 0;
        heatshrink_encoder_sink(&hse, &rawData[sunk], RAW_SIZE - sunk, &this_sunk);
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

    Serial.println("Compression finished.");
    for (int i = 0; i < 20; i++) {
        Serial.print(compressedData[i], HEX);
    }
    Serial.println();
    Serial.print("Compressed data len: "); Serial.println(compressedLen);
}

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

// 🔷 buildPayload
String buildPayload() {
    timestamp = getts(Serial1);
    if (timestamp > 0) {
        Serial.print(F("Unix timestamp: "));
        Serial.println(timestamp);
    } else {
        Serial.println(F("Failed to get UNIX timestamp"));
        timestamp = millis() / 1000;
    }

    // 构造 message = timestamp + device_id + compressedData
    String tsStr = String(timestamp);
    size_t messageLen = tsStr.length() + strlen(DEVICE_ID) + compressedLen;
    uint8_t *message = (uint8_t *)malloc(messageLen);
    if (!message) {
        Serial.println("Message allocation failed!");
        return "";
    }

    // 填充 message
    size_t offset = 0;
    memcpy(message + offset, tsStr.c_str(), tsStr.length());
    offset += tsStr.length();
    memcpy(message + offset, DEVICE_ID, strlen(DEVICE_ID));
    offset += strlen(DEVICE_ID);
    memcpy(message + offset, compressedData, compressedLen);

    Serial.print("messageLen: "); Serial.println(messageLen);
    for (size_t i = 0; i < 10 && i < messageLen; ++i) {
        Serial.print(message[i], HEX); Serial.print(" ");
    }
    Serial.println();

    // 1. 计算 HMAC
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

    // 3. AES加密

    // 4. Base64 编码
    size_t base64Len = (compressedLen + 2) / 3 * 4 + 1;
    char *base64Encoded = (char *)malloc(base64Len);
    if (!base64Encoded) {
        Serial.println("Base64 memory allocation failed!");
        return "";
    }
    memset(base64Encoded, 0, base64Len);
    SimpleBase64::encode(base64Encoded, compressedData, compressedLen);

    // base64Encoded[strlen(base64Encoded)] = '\0';

    // Serial.println("Base64 encoded payload:");
    // Serial.println(base64Encoded);


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
            generateData();
        } else if (cmd == "compress") {
            compressData();
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