//HC12 header files - 통신모듈
#include <SoftwareSerial.h>
SoftwareSerial HC12(10, 11);

//전역변수
bool status = false; // 데이터 전송, 단 분리, 낙하산 사출 시스템 on/off
bool ejectionActive = false;        // 릴레이 작동 중 상태 플래그
bool ejectionDone = true;          // 완료 메시지 한 번만 출력 플래그
bool limitswitch1ON = false; 
bool limitswitch2ON = false; 
bool separated = false;
bool incomplete_separation = false;
int LimitSwitch1 = 4;  // 리미트스위치1 핀 지정	// 리미트스위치, 릴레이모듈 등 각 핀간 거리를 두고 지정할 것..
int LimitSwitch2 = 6;  // 리미트스위치2 핀 지정
int Relay1 = 2;	// 낙하산 사출 릴레이모듈 핀 지정, 차후 수정 필요!!
unsigned long ejectionstartMillis = 0; // 낙하산 사출 릴레이모듈 열선 가열 시작 시간 지정
const long ejectioninterval = 10000; // 릴레이모듈 열선 가열 시간 10000ms로 설정

void setup() {
  pinMode(Relay1, OUTPUT);	// 낙하산 사출 릴레이모듈 출력 지정
  pinMode(LimitSwitch1, INPUT); // 단 분리 확인용 리미트 스위치1 입력 지정
  pinMode(LimitSwitch2, INPUT); // 단 분리 확인용 리미트 스위치2 입력 지정
  Serial.begin(9600);
  HC12.begin(9600);
  Serial.println("This is 2nd Rocket");
  delay(1000);
}

void loop() {
  unsigned long currentMillis = millis(); // 아두이노 작동 시작 시, 밀리스함수 작동 시작 및 변수에 저장됨

  if (!status) {
    if (!digitalRead(LimitSwitch1)) {
      limitswitch1ON = false;
      Serial.println("Limitswitch1 not connected.");
      HC12.println("Limitswitch1 not connected.");
    } 
    if (!digitalRead(LimitSwitch2)) {
      limitswitch2ON = false;
      Serial.println("Limitswitch2 not connected.");
      HC12.println("Limitswitch2 not connected.");
    } 
    if (!limitswitch1ON && digitalRead(LimitSwitch1)) {
      limitswitch1ON = true; // 한번만 출력
      Serial.println("Limitswitch1 connected.");
      HC12.println("Limitswitch1 connected.");
    } 
    if (!limitswitch2ON && digitalRead(LimitSwitch2)) {
      limitswitch2ON = true; // 한번만 출력
      Serial.println("Limitswitch2 connected.");
      HC12.println("Limitswitch2 connected.");
    }
    delay(1000);
  }

  // === [낙하산 사출 전이나 사출 시작 10초 후는 릴레이모듈 off] ===
  if ((currentMillis - ejectionstartMillis) >= ejectioninterval) {  // 낙하산 사출 전이나 사출 시작 10초 후는 릴레이모듈 off
	  digitalWrite(Relay1, LOW);
    ejectionActive = false; // 작동 종료
    if (!ejectionDone) {    // 완료 메시지 한 번만 출력
      Serial.println(">> RELAY OFF.");
      HC12.println(">> RELAY OFF.");
      ejectionDone = true;  // 메시지 출력 표시
    }
  }

  // === [HC-12로 명령 수신] ===
  if (HC12.available()) {
    String input = HC12.readStringUntil('\n');
    input.trim();

    if (input.equalsIgnoreCase("START")) {  // 데이터 송수신 on
      status = true;  
      Serial.println(">> START command received. Data TX enabled.");
		  HC12.println(">> Data TX enabled."); // HC통신은 문장은 최대한 간결하게, 위의 시리얼프린트는 주석처리 할 것.
    } else if (input.equalsIgnoreCase("STOP")) {  // 데이터 송수신 off
      status = false;
      Serial.println(">> STOP command received. Data TX disabled.");
		  HC12.println(">> STOP command received. Data TX disabled.");
    } else if (input.equalsIgnoreCase("EJECT")) { // 낙하산 자동 사출 실패 시 입력코드
      digitalWrite(Relay1, HIGH); // 릴레이 ON
		  ejectionstartMillis = currentMillis;  // 낙하산 사출 릴레이 모듈 작동 시작부터 카운트다운 10초
      ejectionActive = true; 
      ejectionDone = false; // 새로운 EJECT 시도시 완료 메시지 플래그 리셋
		  Serial.println(">> EJECT command received. PARACHUTE EJECTION has started.");
		  HC12.println(">> EJECT command received. PARACHUTE EJECTION has started.");
    } else {
      Serial.print(">> Unknown command received: ");
      Serial.println(input);
    }
  }

  // === [단 분리, 낙하산 사출 시스템 작동 시작] ===
  if (status) {
    // === [단 분리 확인 시 낙하산 사출 릴레이 모듈 작동 시작] === 
    if(!digitalRead(LimitSwitch1) && !digitalRead(LimitSwitch2)) {
		if (!separated) {
			separated = true; // 한 번 실행되면 true로 설정
			incomplete_separation = false;	// 초기화
			Serial.println(">> SEPARATION complete.");
      	HC12.println(">> SEPARATION complete.");
      	digitalWrite(Relay1, HIGH); // 릴레이 ON
      	ejectionstartMillis = currentMillis;  // 낙하산 사출 릴레이 모듈 작동 시작부터 카운트다운 10초
      	ejectionActive = true; 
      	ejectionDone = false; // 새로운 EJECT 시도시 완료 메시지 플래그 리셋
      	Serial.println(">> AUTO EJECTION has started.");
			HC12.println(">> AUTO EJECTION has started.");
		}  
    } else if (digitalRead(LimitSwitch1) ^ digitalRead(LimitSwitch2)) {
		if (!incomplete_separation) {
			incomplete_separation = true;	// 한 번 실행되면 true로 설정
			separated = false;	// 초기화
			Serial.println(">> INCOMPLETE SEPARATION!");
      	HC12.println(">> INCOMPLETE SEPARATION!");
		}
            
    }
  }
}
