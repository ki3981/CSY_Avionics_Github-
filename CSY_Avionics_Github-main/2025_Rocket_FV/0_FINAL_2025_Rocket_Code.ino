#include <BMP180.h>
#include <BMP180DEFS.h>
#include <MetricSystem.h>
#include <EBIMU.h>
#include <math.h>
#include <SD.h>
#include <SPI.h>

#define HC12 Serial2
BMP180 bmp180;

bool status = false;
bool RelayStatus = false;
bool RepeatStatus = true;
bool SDstatus = false; // [NOTE] 남겨두지만 logToSDCard에서는 더 이상 사용 안 함
unsigned long prevDataTxMillis = 0;
const unsigned long dataTxInterval = 500;

bool ejectionActive = false;        // 릴레이 작동 중 상태 플래그
bool ejectionDone = true;           // 완료 메시지 한 번만 출력 플래그
bool limitswitch1ON = false; 
bool limitswitch2ON = false; 
bool separated = false;
bool incomplete_separation = false;
int LimitSwitch1 = 2;  // 리미트스위치1 핀 지정
int LimitSwitch2 = 8;  // 리미트스위치2 핀 지정
int Relay2 = 22;       // Relay2 핀 지정
int Relay3 = 23;       // Relay3 핀 지정
int Relay4 = 24;       // Relay4 핀 지정
unsigned long ejectionstartMillis = 0; // 낙하산 사출 릴레이모듈 열선 가열 시작 시간 지정
const long ejectioninterval = 5000;    // 릴레이모듈 열선 가열 시간 5000ms로 설정

float latestEuler[6] = {0};
bool eulerValid = false;

//초기 자이로 각도값 변수설정
float initGyro[3]={0};
float init_temp = 0;
float init_pres = 0;
float init_alti = 0;

//SD카드 
File myFile;  // SD카드 파일 객체 선언

// ===== [ADD] SD 비차단 로깅 상태/큐 =====
bool sdOK = false;                 // 현재 파일 오픈/사용 가능 여부
bool sdErrorReported = false;      // 동일 에러 중복 알림 억제
unsigned long nextSdRetryMs = 0;   // 재오픈 백오프 타임스탬프
unsigned long sdRetryMs = 1000;    // 1s -> 2s -> 4s ... (최대 8s)
const unsigned long sdRetryMsMax = 8000;
unsigned long lastFlushMs = 0;
const unsigned long sdFlushInterval = 2000; // 2초마다 flush

struct LogQueue {
  static const uint8_t CAP = 16;  // 큐 라인 수
  static const uint8_t W   = 96;  // 한 줄 최대 길이
  char buf[CAP][W];
  uint8_t head = 0, tail = 0;
  bool isFull()  const { return (uint8_t)(head + 1) % CAP == tail; }
  bool isEmpty() const { return head == tail; }
  bool push(const char* s) {
    if (isFull()) return false;
    uint8_t i = head;
    size_t n = 0; 
    while (s[n] && n < W - 1) { buf[i][n] = s[n]; n++; }
    buf[i][n] = '\0';
    head = (uint8_t)(head + 1) % CAP;
    return true;
  }
  bool pop(char out[W]) {
    if (isEmpty()) return false;
    uint8_t i = tail;
    size_t n = 0; 
    while (buf[i][n]) { out[n] = buf[i][n]; n++; }
    out[n] = '\0';
    tail = (uint8_t)(tail + 1) % CAP;
    return true;
  }
} gLogQ;

inline void log_enqueue(const char* s) {
  if (gLogQ.isFull()) {            // [ADD] 꽉 차면 가장 오래된 로그 드롭
    char drop[LogQueue::W]; 
    gLogQ.pop(drop);
  }
  gLogQ.push(s);
}

// ===== [ADD] SD 파일 재오픈(백오프) + 최초 헤더 기록 =====
void tryOpenLog() {
  if (sdOK) return;
  unsigned long now = millis();
  if (now < nextSdRetryMs) return;

  myFile = SD.open("data.txt", FILE_WRITE);
  sdOK = (bool)myFile;

  if (!sdOK) {
    if (!sdErrorReported) {
      Serial.println("SD open failed");
      HC12.println("J");
      sdErrorReported = true;
    }
    nextSdRetryMs = now + sdRetryMs;
    sdRetryMs = min(sdRetryMs * 2, sdRetryMsMax);
  } else {
    if (sdErrorReported) {
      Serial.println("SD recovered");
      HC12.println("K");
    }
    sdErrorReported = false;
    sdRetryMs = 1000;
    lastFlushMs = now;
    myFile.println("===CSY_Flight Data==="); 
    myFile.flush(); // [FIX] flush()는 void. 반환값 체크 없음
  }
}

const uint16_t WRITE_BUDGET_US    = 1000; // [ADD] 루프당 SD I/O 최대 1ms
const uint8_t  MAX_LINES_PER_TICK = 4;    // 루프당 최대 4줄만 씀

// ===== [ADD] 큐 → 파일, 시간/줄 예산 내에서만 처리 =====
void processLogWriter() {
  if (!sdOK) return;
  unsigned long start = micros();
  uint8_t wrote = 0;
  char line[LogQueue::W];

  while (wrote < MAX_LINES_PER_TICK && (micros() - start) < WRITE_BUDGET_US) {
    if (!gLogQ.pop(line)) break;
    if (myFile.println(line) == 0) { // println 실패 시 0
      sdOK = false;
      myFile.close();
      nextSdRetryMs = millis() + sdRetryMs;
      if (!sdErrorReported) { Serial.println("SD write failed"); HC12.println("J"); sdErrorReported = true; }
      return;
    }
    wrote++;
  }

  if (wrote > 0) {
    unsigned long now = millis();
    if (now - lastFlushMs >= sdFlushInterval) {
      myFile.flush(); // [FIX] SD.h의 flush()는 void 반환, 호출만
      lastFlushMs = now;
    }
  }
}

// ===== [REPLACE] SD카드 적는 함수: 즉시 쓰기 → 큐 삽입으로 변경 =====
void logToSDCard(String dataLine) {
  // [DEPRECATED] 이전 방식: 매번 SD.open/println/close → 블로킹
  // [MOD] 지금은 큐에 넣고 바로 복귀 → 메인 루프 타이밍 영향 없음
  char buf[LogQueue::W];
  // 기존 포맷 유지하면서 millis 프리픽스 포함
  snprintf(buf, sizeof(buf), "%lu: %s", millis(), dataLine.c_str());
  log_enqueue(buf);

  // [DEPRECATED] 여기서 SDstatus로 에러 출력/토글하던 코드는 제거
}

//설정함수
void setup() {
  Serial.begin(9600);
  Serial2.begin(9600);  // HC-12
  Serial3.begin(9600);  // EBIMU
  digitalWrite(Relay2, HIGH);
  digitalWrite(Relay3, HIGH);
  digitalWrite(Relay4, HIGH);
  pinMode(Relay2, OUTPUT);	// 1단 낙하산 사출 릴레이모듈 출력 지정
  pinMode(Relay3, OUTPUT);	// 단 분리 릴레이모듈 출력 지정
  pinMode(Relay4, OUTPUT);	// 2단 낙하산 사출 릴레이모듈 출력 지정
  pinMode(LimitSwitch1, INPUT); // 단 분리 확인용 리미트 스위치1 입력 지정
  pinMode(LimitSwitch2, INPUT); // 단 분리 확인용 리미트 스위치2 입력 지정

  if (!bmp180.begin()) {
    Serial.println("BMP180 not found!");
    while (1);
  }

  Serial.println("This is 'B' HC-12 Module");

  // SD 카드 초기화
  delay(500);  // 전원 안정화 대기
  Serial.print("Initializing SD card...");
  HC12.println("O");
  HC12.println("O");

  // [ADD] SPI 마스터 고정 (권장)
  pinMode(53, OUTPUT);
  pinMode(10, OUTPUT);

  // SD 카드 초기화 (CS 핀은 53번을 사용, Mega 기준)
  if (!SD.begin(53)) {
    Serial.println("initialization failed!");
    HC12.println("N");
    HC12.println("N");
    // [MOD] return하지 않고 이후 tryOpenLog()가 백오프 재시도 하도록 상태만 셋업
    sdOK = false;
    nextSdRetryMs = millis() + sdRetryMs;
  } else {
    Serial.println("initialization done.");
    HC12.println("M");
    HC12.println("M");
    // [MOD] 파일 오픈을 유지하도록 한 번 시도 (실패해도 루프에서 재시도)
    tryOpenLog();

    // [DEPRECATED] setup에서 매번 열고 닫으며 헤더 쓰던 코드는 제거
  }
}

void loop() {

  // EBIMU 데이터는 항상 읽어서 최신값 저장
  if (EBimuAsciiParser(Serial3, latestEuler, 6)) {
    eulerValid = true;
  }
  unsigned long now = millis();

  if (!status) {
    if (!digitalRead(LimitSwitch1)) {
      limitswitch1ON = false;
      Serial.println("Limitswitch1 not connected.");
      HC12.println("Q");
    } 
    if (!digitalRead(LimitSwitch2)) {
      limitswitch2ON = false;
      Serial.println("Limitswitch2 not connected.");
      HC12.println("R");
    } 
    if (!limitswitch1ON && digitalRead(LimitSwitch1)) {
      limitswitch1ON = true; // 한번만 출력
      Serial.println("Limitswitch1 connected.");
      HC12.println("S");
    } 
    if (!limitswitch2ON && digitalRead(LimitSwitch2)) {
      limitswitch2ON = true; // 한번만 출력
      Serial.println("Limitswitch2 connected.");
      HC12.println("T");
    }
    delay(1000);
  }

  if ((now - ejectionstartMillis) >= ejectioninterval) {  // 낙하산 사출 전이나 사출 시작 10초 후는 릴레이모듈 off
	  digitalWrite(Relay2, HIGH);
    digitalWrite(Relay3, HIGH);
    digitalWrite(Relay4, HIGH);
    ejectionActive = false; // 작동 종료
    if (!ejectionDone) {    // 완료 메시지 한 번만 출력
      Serial.println(">> RELAY OFF.");
      logToSDCard(String(millis())+": [AUTO]릴레이모듈 외부전원 공급 꺼짐.");
      HC12.println("U");
      HC12.println("U");
      ejectionDone = true;  // 메시지 출력 표시
    }
  }

  // 명령 수신 (지금은 기존 블로킹 방식 유지. 필요시 비블로킹으로 바꿔줄 수 있음)
  if (HC12.available()) {
    String cmd = HC12.readStringUntil('\n');
    cmd.replace("\r", "");  // 윈도우 CRLF 문제 방지
    cmd.trim();             // 공백 제거

    if (cmd == "0") {
      status = true;
      Serial.println(">>[CMD] START command received. Data TX enabled.");
      logToSDCard(String(millis())+": >>[CMD] 시작 명령 수신. 데이터 송신 시작.+자이로 캘리브레이션 실시");
      initGyro[0] = latestEuler[0];
      initGyro[1] = latestEuler[1];
      initGyro[2] = latestEuler[2];
      init_temp = bmp180.readTemperature();
      init_pres = bmp180.readPressure();
      init_alti = bmp180.readAltitude();
    } else if (cmd == "1") {
      status = false;
      Serial.println(">>[CMD] STOP command received. Data TX disabled.");
      logToSDCard(String(millis())+": >>[CMD] 정지 명령 수신. 데이터 송신 중단.");
    } else if (cmd == "2") {
      RelayStatus = true;
      Serial.println(">>[CMD] WARNING!!! RELAY MODULE ACTIVATED!!.");
      logToSDCard(String(millis())+": >>========[CMD] 경고!! 릴레이 모듈 활성화됨!!+발사준비상태 돌입!!======================");
      HC12.println("Z");
      HC12.println("Z");
    } else if (cmd == "3") {
      RelayStatus = false;
      Serial.println(">>[CMD] Relay module deactivated.");
      logToSDCard(String(millis())+": >>[CMD] 릴레이 모듈 비활성화");
      HC12.println("Y");
      HC12.println("Y");
    } else if (cmd == "4") {
      Serial.println(">>[CMD] FORCED 1ST PARACHUTE LAUNCH ACTIVATED!!"); //1단 낙하산 강제사출
      logToSDCard(String(millis())+": >>[CMD] 강제 1단부 낙하산 사출 실시!");
      HC12.println("B");
      HC12.println("B");
      digitalWrite(Relay2, LOW); // 릴레이2 ON
      ejectionstartMillis = now;  // 낙하산 사출 릴레이 모듈 작동 시작부터 카운트다운 10초
      ejectionActive = true; 
      ejectionDone = false; // 새로운 EJECT 시도시 완료 메시지 플래그 리셋
    } else if (cmd == "5") {
      Serial.println(">>[CMD] FORCED SEPARATION RELAY MODULE ACTIVATED!!"); //단분리 강제시작
      logToSDCard(String(millis())+": >>[CMD] 강제 단분리 릴레이모듈 가동!");
      HC12.println("C");
      HC12.println("C");
      digitalWrite(Relay3, LOW); // 릴레이3 ON
      ejectionstartMillis = now;  // 낙하산 사출 릴레이 모듈 작동 시작부터 카운트다운 10초
      ejectionActive = true; 
      ejectionDone = false; // 새로운 EJECT 시도시 완료 메시지 플래그 리셋
    } else if (cmd == "6") {
      Serial.println(">>[CMD] FORCED 2ND PARACHUTE LAUNCH ACTIVATED!!"); //2단 낙하산 강제사출
      logToSDCard(String(millis())+": >>[CMD] 강제 2단부 낙하산 사출 실시!!");
      HC12.println("D");
      HC12.println("D");
      digitalWrite(Relay4, LOW); // 릴레이4 ON
      ejectionstartMillis = now;  // 낙하산 사출 릴레이 모듈 작동 시작부터 카운트다운 10초
      ejectionActive = true; 
      ejectionDone = false; // 새로운 EJECT 시도시 완료 메시지 플래그 리셋
    } else if (cmd == "7") {
      Serial.println(">>[CMD] FORCED ALL RELAY MODULE ACTIVATED!!"); //모든 릴레이모듈 강제가동
      logToSDCard(String(millis())+": >>[CMD] 모든 릴레이 모듈 강제 활성화!!");
      HC12.println("E");
      HC12.println("E");
      digitalWrite(Relay2, LOW); // 릴레이2 ON
      digitalWrite(Relay3, LOW); // 릴레이3 ON
      digitalWrite(Relay4, LOW); // 릴레이4 ON
      ejectionstartMillis = now;  // 낙하산 사출 릴레이 모듈 작동 시작부터 카운트다운 10초
      ejectionActive = true; 
      ejectionDone = false; // 새로운 EJECT 시도시 완료 메시지 플래그 리셋
    } else if (cmd == "8") {
      Serial.println(">>[CMD] Reset RepeatStatus"); //릴레이모듈 부울리언 리셋
      logToSDCard(String(millis())+": >>[CMD] RepeatStatus 리셋 실시");
      HC12.println("P");
      HC12.println("P");
      RepeatStatus = true;
    } else {
      Serial.print(">>[CMD] Unknown command: ");
      Serial.println(cmd);
      logToSDCard(String(millis())+": >>[CMD] 이해할 수 없는 명령: ");
      logToSDCard(String(millis())+": "+cmd);
    }
    Serial.println(cmd);
    logToSDCard(String(millis())+"수신된 명령어[RAW]: "+cmd);
  }

  // 주기적 데이터 송신
  if (status && (now - prevDataTxMillis >= dataTxInterval)) {
    prevDataTxMillis = now;
    float temp = bmp180.readTemperature();
    float pres = bmp180.readPressure();
    float alti = bmp180.readAltitude()-init_alti;

    String msg = String(alti, 1); //패킷작성
    //EBIMU 센서값 전송(첫번째는 각도, 두번째는 지평좌표계 기준의 속도)
    for (int i = 0; i < 3; i++) {
      msg += "," + String(latestEuler[i]-initGyro[i], 0);
    }

    for (int i = 3; i < 6; i++) {
      msg += ", " + String(latestEuler[i], 1);
    }

    HC12.println(msg);
    Serial.println(msg);
    logToSDCard(String(millis())+": "+msg);

    //낙하산 사출 및 단분리 조건(자이로)
    if(RelayStatus){
      if (tan((latestEuler[0]-initGyro[0])*0.0174533)*tan((latestEuler[0]-initGyro[0])*0.0174533) + tan((latestEuler[1]-initGyro[1])*0.0174533)*tan((latestEuler[1]-initGyro[1])*0.0174533) >= tan(35*0.0174533)*tan(35*0.0174533)) {
       if(RepeatStatus){
       logToSDCard(String(millis())+": [AUTO]설정 자이로각도 도달에 따른 자동 낙하산 사출 및 단분리 실시");
       HC12.println("A");
       HC12.println("A");
       RepeatStatus = false;
       digitalWrite(Relay2, LOW); // 릴레이2 ON
       digitalWrite(Relay3, LOW); // 릴레이3 ON
       digitalWrite(Relay4, LOW); // 릴레이4 ON
       ejectionstartMillis = now;  // 낙하산 사출 릴레이 모듈 작동 시작부터 카운트다운 10초
       ejectionActive = true; 
       ejectionDone = false; // 새로운 EJECT 시도시 완료 메시지 플래그 리셋
       }
      }
        // === [단 분리 확인 시 낙하산 사출 릴레이 모듈 작동 시작] === 
      if(!digitalRead(LimitSwitch1) && !digitalRead(LimitSwitch2)) {
		    if (!separated) {
			    separated = true; // 한 번 실행되면 true로 설정
			    incomplete_separation = false;	// 초기화
			    Serial.println(">> SEPARATION complete.");
          logToSDCard(String(millis())+": >> 단분리 성공");
      	  HC12.println("X");
          HC12.println("X");
		    }
      } else if (digitalRead(LimitSwitch1) ^ digitalRead(LimitSwitch2)) {
		      if (!incomplete_separation) {
			    incomplete_separation = true;	// 한 번 실행되면 true로 설정
			    separated = false;	// 초기화
			    Serial.println(">> INCOMPLETE SEPARATION!");
          logToSDCard(String(millis())+": >> 불완전한 단분리!!");
      	  HC12.println("V");
          HC12.println("V");
		      }
        }
    }
  }

  // ===== [ADD] SD 상태 복구 & 백그라운드 라이터: 루프 맨 끝에서 아주 짧게 처리 =====
  if (!sdOK) tryOpenLog();   // 필요 시 백오프 재오픈
  processLogWriter();        // 큐에서 조금만 꺼내 씀(시간예산 내)
}
