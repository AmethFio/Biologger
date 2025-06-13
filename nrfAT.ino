#include <Arduino.h>
#include <Adafruit_TinyUSB.h>

void setup() {
  // 启动 USB 串口
  Serial.begin(115200);
  while (!Serial) delay(10);
  Serial.println("Serial bridge setting up");
  Serial.print("Serial1 RX: "); Serial.println(PIN_SERIAL1_RX);
  Serial.print("Serial1 TX: "); Serial.println(PIN_SERIAL1_TX);
  Serial1.begin(115200);  // nRF9160
  Serial.println("Serial bridge ready. Type your AT commands:");
}

void sendAT(const char* cmd, unsigned long waitTime = 500) {
  Serial.print("[TX] ");
  Serial.println(cmd);
  Serial1.println(cmd);

  unsigned long start = millis();
  while (millis() - start < waitTime) {
    while (Serial1.available()) {
      char c = Serial1.read();
      Serial.write(c);  // 打印模块返回内容
    }
  }
}

void handleSetupCommand() {
  Serial.println("[Trigger] Setting up...");
  sendAT("AT+CFUN=4", 500);
  sendAT("AT%XSYSTEMMODE=1,0,0,0", 500);
  sendAT("AT%XBANDLOCK=1,\"1100000000010000000\"", 500);
  sendAT("AT+CGDCONT=0,\"IP\",\"soracom.io\",\"sora\",\"sora\"", 500);
  sendAT("AT+CFUN=1", 1000);  // 开机后稍长延迟
  sendAT("AT+CEREG?", 1000);
  sendAT("AT+CGDCONT?", 1000);
  sendAT("AT+CGACT?", 1000);
  Serial.println("Waiting for register...");
  delay(2000);
  sendAT("AT%XMONITOR", 500);
}

void loop() {

  static String buffer;
  // 从 USB 收数据，发给 nRF9160
  while (Serial.available()) {
    char c = Serial.read();
    Serial.write(c);
    Serial1.write(c);

    buffer += c;
    if (c == '\n' || c == '\r') {

      buffer.trim();
      if (buffer == "HTTP") {
        Serial.println("[Trigger] Sending HTTP AT command to nRF9160");
        Serial1.println(R"(AT+HTTPCREQ="https://httpbin.org/get")");
      }

      if (buffer == "SAKURA"){
        Serial.println("[Trigger] HTTP request to sakura.ad.jp");
        sendAT(R"(AT#XHTTPCCON=1,"www.sakura.ad.jp",80)", 500);
        delay(2000);
        sendAT(R"(AT#XHTTPCREQ="GET","/","")", 500);
        delay(5000);
        sendAT(R"(AT#XHTTPCCON=0)", 500);
      }

      if (buffer == "POSTMAN"){
        Serial.println("[Trigger] HTTP request to postman-echo.com");
        sendAT(R"(AT#XHTTPCCON=1,"postman-echo.com",80)", 500);
        delay(2000);
        sendAT(R"(AT#XHTTPCREQ="GET","/get?foo=bar","")", 500);
        delay(5000);
        sendAT(R"(AT#XHTTPCCON=0)", 500);
      }

      if (buffer == "SETUP") {
        handleSetupCommand();
      }

      buffer = "";
    }
  }

  // 从 nRF9160 收数据，发回 USB
  while (Serial1.available()) {
    char c = Serial1.read();
    Serial.write(c);
  }

}
