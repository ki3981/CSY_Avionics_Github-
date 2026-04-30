#define HC12 Serial2
#define Uno Serial1

//이 코드는 지상관제용 보드 중 MEGA보드용으로, HC-12로 데이터를 받아서 UNO보드로 수신명령어를 전송하고 시리얼에는 데이터(숫자)만 출력하는 코드입니다.(시리얼 스튜디오용)

unsigned long lastReceiveTime = 0;
bool skipNextLine = false;

void setup() {
  Serial.begin(9600);
  HC12.begin(9600);
  Uno.begin(9600);
  Serial.println("This is the GROUND board.");
  Uno.println("This is the GROUND board.");
}

void loop() {
  // [1] 로켓으로부터 수신된 데이터 출력
  while (HC12.available()) {
    String recv = HC12.readStringUntil('\n');
    recv.trim();             

    if (skipNextLine) {
      skipNextLine = false; // 깨진 1줄 무시
      continue;
    }
    if(recv=="A"){
    Uno.println(">>>[RX]STARTING PARACHUTE EJECTION AND SEPARATION");
    }else if(recv=="B"){
    Uno.println(">>>[RX]FORCED 1ST PARACHUTE LAUNCH ACTIVATED!!");
    }else if(recv=="C"){
    Uno.println(">>>[RX]FORCED SEPARATION RELAY MODULE ACTIVATED!!");
    }else if(recv=="D"){
    Uno.println(">>>[RX]FORCED 2ND PARACHUTE LAUNCH ACTIVATED!!");
    }else if(recv=="E"){
    Uno.println(">>>[RX]ALL RELAY MODULE IS TURNED ON!!!");
    }else if(recv=="Z"){
    Uno.println(">>>[RX]CONFIRMED: RELAY MODULED ACTIVATED");
    }else if(recv=="Y"){
    Uno.println(">>>[RX]CONFIRMED: [DEACT.]RELAY MODULED DEACTIVATED");
    }else if(recv=="X"){
    Uno.println(">>>[RX]SERPARATION COMPLETED");
    }else if(recv=="V"){
    Uno.println(">>>[RX]!! INCOMPLETE SPERATION!!");
    }else if(recv=="U"){
    Uno.println(">>>[RX]RELAY MODULE POWER SUPPLY OFF");
    }else if(recv=="T"){
    Uno.println(">>>[RX]LIMIT SWITCH 2 CONNECTED");
    }else if(recv=="S"){
    Uno.println(">>>[RX]LIMIT SWITCH 1 CONNECTED");
    }else if(recv=="R"){
    Uno.println(">>>[RX] !!LIMIT SWITCH 2 NOT CONNECTED!!");
    }else if(recv=="Q"){
    Uno.println(">>>[RX] !!LIMIT SWITCH 1 NOT CONNECTED!!");
    }else if(recv=="P"){
    Uno.println(">>>[RX] GYRO BOOLEAN RESETTED");
    }else if(recv=="O"){
    Uno.println(">>>[RX] INITIALIZING SD CARD...");
    }else if(recv=="N"){
    Uno.println(">>>[RX] !!FAILED INITIALIZING SD CARD!!");
    }else if(recv=="M"){
    Uno.println(">>>[RX] SUCCESSFULLY INITIALIZED SD CARD");
    }else if(recv=="L"){
    Uno.println(">>>[RX] !CAN'T OPEN DATA.TXT FILE!");
    }else if(recv=="K"){
    Uno.println(">>>[RX] DATA.TXT FILE IS READY");
    }else if(recv=="J"){
    Uno.println(">>>[RX] !!ERROR, CAN'T WRITE ON DATA.TXT FILE!!");
    }else{
    Serial.println(recv);
    }
    lastReceiveTime = millis(); // 수신 시간 업데이트
  }

  // [2] 사용자 명령 입력 처리
  while (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();


    // 경과 시간 확인
    unsigned long now = millis();
    unsigned long delta = now - lastReceiveTime;

    // 조건에 따라 대기
    if (delta > 350) {
      delay(200); // 350ms 넘었으면 200ms 추가 대기
    }
    // 350ms 이내면 즉시 전송

    // 명령 전송
    HC12.println(cmd);
    if(cmd=="0"){
      Uno.println(">>[CMD SENT] START COMMAND SENT");
    }else if(cmd=="1"){
     Uno.println(">>[CMD SENT] STOP COMMAND SENT");
    }else if(cmd=="2"){
     Uno.println(">>[CMD SENT] WARNING!! RELAY MODULE ACTIVATED!!!");
    }else if(cmd=="3"){
     Uno.println(">>[CMD SENT] Relay module deactivated");
    }else if(cmd=="4"){
     Uno.println(">>[CMD SENT] FORCED 1ST PARACHUTE LAUNCH ACTIVATED!!");
    }else if(cmd=="5"){
     Uno.println(">>[CMD SENT] FORCED SEPARATION RELAY MODULE ACTIVATED!!");
    }else if(cmd=="6"){
     Uno.println(">>[CMD SENT] FORCED 2ND PARACHUTE LAUNCH ACTIVATED!!");
    }else if(cmd=="7"){
     Uno.println(">>[CMD SENT] FORCED ALL RELAY MODULE ACTIVATED!!");
    }else{
      Uno.println(">>[CMD SENT] UNKNOWN COMMAND");
    }
    skipNextLine = true; // 바로 다음 수신되는 줄은 무시
  }
}
