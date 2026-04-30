bool separated = false;  // 분리 완료 여부
unsigned long lastIncompletePrintTime = 0; // 마지막 출력 시각 저장
const unsigned long PRINT_INTERVAL = 1000; // 1초 간격

void loop() {
  unsigned long currentMillis = millis();

  // 1. 완전 분리된 경우 — 딱 한 번만 실행
  if (!digitalRead(LimitSwitch1) && !digitalRead(LimitSwitch2)) {
    if (!separated) { // 아직 처리 안됐으면
      Serial.println(">> SEPARATION complete.");
      HC12.println(">> SEPARATION complete.");
      digitalWrite(Relay1, HIGH); // 릴레이 ON
      ejectionstartMillis = currentMillis;
      ejectionActive = true;
      ejectionDone = false;
      Serial.println(">> AUTO EJECTION has started.");
      HC12.println(">> AUTO EJECTION has started.");
      separated = true; // 한 번 실행되면 true로 설정
    }
  }

  // 2. 하나만 눌린 상태 — 1초마다 메시지 출력
  else if (digitalRead(LimitSwitch1) ^ digitalRead(LimitSwitch2)) {
    if (currentMillis - lastIncompletePrintTime >= PRINT_INTERVAL) {
      lastIncompletePrintTime = currentMillis;
      Serial.println(">> SEPARATION incomplete.");
      HC12.println(">> SEPARATION incomplete.");
    }
    separated = false; // 다시 되돌릴 수 있게 초기화 (선택적)
  }

  // 이후 낙하산 eject 완료 후 처리 등은 별도 구현
}
