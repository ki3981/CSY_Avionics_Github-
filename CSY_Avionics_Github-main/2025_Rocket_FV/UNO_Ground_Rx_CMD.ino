#include <SoftwareSerial.h>

//이 코드는 지상관제용 보드 중 UNO보드로, MEGA로부터 받은 수신명령어를 시리얼에 출력해주는 코드입니다. 타임코드를 켜놓고 명령 수신 일시를 확인하세요

SoftwareSerial MegaSerial(10, 11);  // RX, TX (Mega의 TX1 ↔ UNO D10, RX1 ↔ D11)

void setup() {
  Serial.begin(9600);         // PC용 시리얼
  MegaSerial.begin(9600);     // Mega와 통신용
}

void loop() {
  if (MegaSerial.available()) {
    String received = MegaSerial.readStringUntil('\n');
    Serial.println(received);
  }
}
