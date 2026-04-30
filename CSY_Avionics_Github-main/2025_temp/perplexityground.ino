#include <SoftwareSerial.h>
SoftwareSerial HC12(10, 11); // HC-12 TX, RX

void setup() {
  Serial.begin(9600);
  HC12.begin(9600);
  Serial.println("This is the GROUND board.");
}

void loop() {
  // 로켓 데이터 수신 및 PC에 표시
  while (HC12.available()) {
    String recv = HC12.readStringUntil('\n');
    Serial.println("[ROCKET] " + recv);
  }

  // 사용자가 노트북 시리얼 입력창에 명령 입력 시 전송
  while (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    HC12.println(cmd);
    Serial.println("[CMD SENT] " + cmd);
  }
}
