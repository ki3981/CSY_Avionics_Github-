//BMP180 header files - 고도 센서
#include <BMP180.h>
#include <BMP180DEFS.h>
#include <MetricSystem.h>

//HC12 header files - 통신모듈
#include <SoftwareSerial.h>
SoftwareSerial HC12(10, 11);  // TX, RX
BMP180 bmp180;

//BNO055 header files - 자이로모듈
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>

//전역변수
Adafruit_BNO055 bno = Adafruit_BNO055(55);
bool status = false; // 데이터 전송, 단 분리, 낙하산 사출 시스템 on/off
bool separationActive = false;        // 릴레이 작동 중 상태 플래그
bool separationDone = true;          // 완료 메시지 한 번만 출력 플래그
bool ejectionActive = false;        // 릴레이 작동 중 상태 플래그
bool ejectionDone = true;          // 완료 메시지 한 번만 출력 플래그

unsigned long prevDataTxMillis = 0;     // [수정] 데이터 송신 타이밍
const unsigned long dataTxInterval = 5; // [수정] 데이터 송신 주기(5ms)
int Relay1 = 3; // 단 분리 릴레임모듈 핀 지정, 차후 수정 필요!!
int Relay2 = 6;	// 낙하산 사출 릴레이모듈 핀 지정, 차후 수정 필요!!
unsigned long separationstartMillis = 0; // 단 분리 릴레이모듈 열선 가열 시작 시간 지정
unsigned long ejectionstartMillis = 0; // 낙하산 사출 릴레이모듈 열선 가열 시작 시간 지정
const long ejectioninterval = 10000; // 릴레이모듈 열선 가열 시간 10000ms로 설정
const float angleThreshold = 45; // 단 분리 또는 낙하산 사출 조건1: 각도(단위: degree), 여러 개 지정 가능.
const float altitudeThreshold = 500.0; // 단 분리 또는 낙하산 사출 조건2: 높이(단위: m), 여러 개 지정 가능.

void setup() {
  Serial.begin(9600);
  if (!bmp180.begin()) {
    Serial.println("BMP180 not found!");
    while (1);
  }
  HC12.begin(9600);
  Serial.println("This is 'B' HC-12 Module");

  if(!bno.begin()) {
    Serial.print("NO BNO055 DETECTED! ... Check your wiring or I2C ADDR!");
    while(1);
  }
  pinMode(Relay1, OUTPUT);	// 단 분리 릴레이모듈 출력 지정
  pinMode(Relay2, OUTPUT);	// 낙하산 사출 릴레이모듈 출력 지정
  delay(1000);
  bno.setExtCrystalUse(true);
}

void loop() {
  // === [HC-12로 명령 수신] ===
  unsigned long currentMillis = millis(); // 아두이노 작동 시작 시, 밀리스함수 작동 시작 및 변수에 저장됨
  
  if ((currentMillis - separationstartMillis) >= ejectioninterval) {  // 단 분리 전이나 분리 시작 10초 후는 릴레이모듈 off
	  digitalWrite(Relay1, LOW);
    separationActive = false; // 작동 종료
    if (!separationDone) {    // 완료 메시지 한 번만 출력
      Serial.println(">> SEPARATION complete. RELAY OFF.");
      HC12.println(">> SEPARATION complete. RELAY OFF.");
      separationDone = true;  // 메시지 출력 표시
    }
  }

  if ((currentMillis - ejectionstartMillis) >= ejectioninterval) {  // 낙하산 사출 전이나 사출 시작 10초 후는 릴레이모듈 off
	  digitalWrite(Relay2, LOW);
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

    if (input.equalsIgnoreCase("START")) {  // 데이터 송수신 on
      status = true;  
      Serial.println(">> START command received. Data TX enabled.");
		  HC12.println(">> Data TX enabled."); // HC통신은 문장은 최대한 간결하게, 위의 시리얼프린트는 주석처리 할 것.
    } else if (input.equalsIgnoreCase("STOP")) {  // 데이터 송수신 off
      status = false;
      Serial.println(">> STOP command received. Data TX disabled.");
		  HC12.println(">> STOP command received. Data TX disabled.");
    } else if (input.equalsIgnoreCase("SEPARATE")) {  // 자동 단 분리 실패 시 입력코드
      digitalWrite(Relay1, HIGH); // 릴레이 ON
		  separationstartMillis = currentMillis;  // 단 분리 릴레이 모듈 작동 시작부터 카운트다운 10초
      separationActive = true; 
      separationDone = false; // 새로운 SEPARATE 시도시 완료 메시지 플래그 리셋
		  Serial.println(">> SEPARATE command received. SEPARATION has started.");
		  HC12.println(">> SEPARATE command received. SEPARATION has started.");
    } else if (input.equalsIgnoreCase("EJECT")) { // 낙하산 자동 사출 실패 시 입력코드
      digitalWrite(Relay2, HIGH); // 릴레이 ON
		  ejectionstartMillis = currentMillis;  // 낙하산 사출 릴레이 모듈 작동 시작부터 카운트다운 10초
      ejectionActive = true; 
      ejectionDone = false; // 새로운 EJECT 시도시 완료 메시지 플래그 리셋
		  Serial.println(">> EJECT command received. PARACHUTE EJECTION has started.");
		  HC12.println(">> EJECT command received. PARACHUTE EJECTION has started.");
    } else {
      Serial.print(">> Unknown command received: ");
		  HC12.println(">> Unknown command received: ");
      Serial.println(input);
    }
  }

  // [추가: 시리얼에서 입력 받아 HC12로 전송]
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() > 0) {
      HC12.println(cmd);
      Serial.print("[CMD TX to GroundBoard]: ");
      Serial.println(cmd);
    }
  }

  // === [주기적 센서 데이터 송신, millis() 사용, 단 분리, 낙하산 사출 시스템 작동 시작] ===
  unsigned long now = millis();      // [수정] 현재 ms값

  if (status && (now - prevDataTxMillis >= dataTxInterval)) {
    prevDataTxMillis = now;

    // 센서 데이터 송신
    String BMP180temp = String(bmp180.readTemperature(), 4);
    String BMP180Pres = String(bmp180.readPressure(), 4);
    String BMP180Alti = String(bmp180.readAltitude(), 4);

    HC12.print("TEMP:"); HC12.print(BMP180temp); HC12.println(" C");
    HC12.print("PRESS:"); HC12.print(BMP180Pres); HC12.println(" Pa");
    HC12.print("ALTI:"); HC12.print(BMP180Alti); HC12.println(" m");

    sensors_event_t event;
    bno.getEvent(&event);
    imu::Vector<3> LACC = bno.getVector(Adafruit_BNO055::VECTOR_LINEARACCEL);

    String GyroX = String(event.orientation.x,2);
    String GyroY = String(event.orientation.y,2);
    String GyroZ = String(event.orientation.z,2);

    HC12.print("GYRO X:"); HC12.print(GyroX);
    HC12.print(" Y:"); HC12.print(GyroY);
    HC12.print(" Z:"); HC12.println(GyroZ);

    HC12.print("ACC X:"); HC12.print(LACC.x());
    HC12.print(" Y:"); HC12.print(LACC.y());
    HC12.print(" Z:"); HC12.println(LACC.z());

    // === [특정 조건 만족 시 단 분리] ===
    float Pitch = event.orientation.y; // 변수형은 예시일 뿐, 변경 가능. 또한 x,y,z 설정도 차후 설계에 따라 변경할 것.
    float Yaw = event.orientation.z;
    float altitude = bmp180.readAltitude(); // 표준 대기압 기준 고도(m)
    if((Pitch >= angleThreshold)||(Pitch <= -angleThreshold)||(Yaw >= angleThreshold)||(Yaw <= -angleThreshold)||(altitude >= altitudeThreshold)) {
      digitalWrite(Relay1, HIGH); // 릴레이 ON
      separationstartMillis = currentMillis;  // 단 분리 릴레이 모듈 작동 시작부터 카운트다운 10초
      separationActive = true; 
      separationDone = false; // 새로운 SEPARATE 시도시 완료 메시지 플래그 리셋
      Serial.println(">> AUTO SEPARATION has started.");
		  HC12.println(">> AUTO SEPARATION has started.");
    }  // 어쩌다보니 조건문이 많이 붙게 되었는데, 이 중에 가장 신뢰할만한 부분만 남기고 지우는게 좋을 듯함.

    // === [특정 조건 만족 시 낙하산 사출] === 
    if((Pitch >= angleThreshold)||(Pitch <= -angleThreshold)||(Yaw >= angleThreshold)||(Yaw <= -angleThreshold)||(altitude >= altitudeThreshold)) {
      digitalWrite(Relay2, HIGH); // 릴레이 ON
      ejectionstartMillis = currentMillis;  // 낙하산 사출 릴레이 모듈 작동 시작부터 카운트다운 10초
      ejectionActive = true; 
      ejectionDone = false; // 새로운 EJECT 시도시 완료 메시지 플래그 리셋
      Serial.println(">> AUTO EJECTION has started.");
		  HC12.println(">> AUTO EJECTION has started.");
    } // 낙하산 사출 조건은 나중에 리미트 스위치를 조건문에 활용할 예정. 아마도
  }
}
