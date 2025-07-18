// UARTEStream 検証用スケッチ（改善版）
#include <Adafruit_TinyUSB.h>
#include <Arduino.h>
#include "UARTEStream.h"

UARTEStream SerialUART;

void setup() {
  Serial.begin(115200);
  while (!Serial);
  delay(100);

  Serial.println("UARTEStream test start");

  // Print 検証
  // SerialUART.begin(4, 31, 115200, 1);
  //SerialUART.println("Hello from Feather");
  //SerialUART.print("Print test: ");
  //SerialUART.print(12345);
  //SerialUART.print(", float: ");
  //SerialUART.println(3.14159, 3);
  SerialUART.begin(25, 24, 115200, 1);
  SerialUART.print("AT");
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




void loop() {
  static String line = "";
  String input;

  while (Serial.available()) {
    char c = Serial.read();
    if (c != '\n'){
      line += c;
    }
    else {
      input = line;
      line = "";

      if (input.length() == 0) {
        return;
      }
      testfuncs(input);
      }
    }
  }

