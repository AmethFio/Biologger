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
  SerialUART.begin(25, 24, 115200, 1);
  // Print 検証
  //SerialUART.begin(4, 31, 115200, 1);
  //SerialUART.println("Hello from Feather");
  //SerialUART.print("Print test: ");
  //SerialUART.print(12345);
  //SerialUART.print(", float: ");
  //SerialUART.println(3.14159, 3);
}

void loop() {
  static String line = "";
  String input;

  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      input = line;
      line = "";

      if (input.length() > 0) {
        char command = input.charAt(0);
        String args = input.substring(2);

        switch (command) {
          case '1':
            Serial.print("Available: ");
            Serial.println(SerialUART.available());
            break;

          case '2':
            Serial.print("Read: ");
            if (SerialUART.available()) {
              Serial.println((char)SerialUART.read());
            } else {
              Serial.println("nothing");
            }
            break;

          case '3':
            Serial.print("Read String: ");
            Serial.println(SerialUART.readString());
            break;

          case '4': {
            Serial.print("Read Bytes: ");
            char buffer[64];
            size_t len = SerialUART.readBytes(buffer, sizeof(buffer));
            buffer[len] = '\0';
            Serial.println(buffer);
            break;
          }

          case '5':
            Serial.print("Read Until CR: ");
            Serial.println(SerialUART.readStringUntil('\r'));
            break;

          case '6':
            Serial.println("Flush");
            SerialUART.flush();
            break;

          case '7':
            Serial.println("End");
            SerialUART.end();
            break;

          case '8': {
            int comma1 = args.indexOf(',');
            int comma2 = args.indexOf(',', comma1 + 1);
            int comma3 = args.indexOf(',', comma2 + 1);

            if (comma1 > 0 && comma2 > comma1 && comma3 > comma2) {
              int tx = args.substring(0, comma1).toInt();
              int rx = args.substring(comma1 + 1, comma2).toInt();
              int baud = args.substring(comma2 + 1, comma3).toInt();
              int chunk = args.substring(comma3 + 1).toInt();

              if(chunk <= 0 || chunk > 64){
                Serial.println(baud);
                Serial.print(chunk);
                Serial.println("Invalid chunk size (must be 1-64)");
              }  else {
                SerialUART.begin(tx, rx, baud, chunk);
                Serial.println("begin completed");
                SerialUART.println("Serial start");
              }
             }else{
              Serial.println("Invalid format. Use: 8,<tx>,<rx>,<baud>,<chunk>");
             } 
            break;
          }

          case '9':
            Serial.println("Sending raw:");
            Serial.println(args);
            //SerialUART.println(args);
            for (int i = 0; i < args.length(); i++) {
              SerialUART.write((uint8_t)args[i]);
            }
            // 手动加 \r，如果设备需要
            SerialUART.write('\n');
            SerialUART.write('\r');
            break;

          default:
            Serial.println("Unknown command");
            break;
        }
      }
    } else {
      line += c;
    }
  }
}
