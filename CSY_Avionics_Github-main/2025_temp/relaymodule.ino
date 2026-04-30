//HC12 header files - 통신모듈
#include <SoftwareSerial.h>
SoftwareSerial HC12(10, 11);

int Relay = 3; // 릴레이모듈 핀 지정
unsigned long ejectionstartMillis = 0; // 릴레이모듈 열선 가열 시작 시간 지정
const long ejectioninterval = 10000; // 릴레이모듈 열선 가열 시간 10000ms로 설정
bool ejectionActive = false;        // 릴레이 작동 중 상태 플래그
bool ejectionDone = true;          // 완료 메시지 한 번만 출력 플래그

void setup() {
  pinMode(Relay, OUTPUT); // 릴레이모듈 출력 지정
  Serial.begin(9600);
}

void loop() {
  unsigned long currentMillis = millis(); // 밀리스함수로 현재시각 받기
  if ((currentMillis - ejectionstartMillis) >= ejectioninterval) {  // 단 분리 전이나 분리 시작 10초 후는 릴레이모듈 off
	  digitalWrite(Relay, LOW);
    ejectionActive = false; // 작동 종료
    if (!ejectionDone) {    // 완료 메시지 한 번만 출력
        Serial.println(">> PARACHUTE EJECTION complete. RELAY OFF.");
        HC12.println(">> PARACHUTE EJECTION complete. RELAY OFF.");
        ejectionDone = true;  // 메시지 출력 표시
    }
  }

  if (HC12.available()) {
    String input = HC12.readStringUntil('\n');
    input.trim();

    if (input.equalsIgnoreCase("EJECT")) { // 낙하산 자동 사출 실패 시 입력코드이고, 이 조건문 안에 다른 조건을 넣어서 실행가능
    digitalWrite(Relay, HIGH);
    ejectionstartMillis = currentMillis;
    ejectionActive = true; 
    ejectionDone = false; // 새로운 EJECT 시도시 완료 메시지 플래그 리셋
    Serial.println(">> EJECT command received. PARACHUTE EJECTION has started.");
    HC12.println(">> EJECT command received. PARACHUTE EJECTION has started.");
    }
  }
}
