int Relay = 3;
 
void setup(){
  pinMode(Relay,OUTPUT);         // 릴레이를 출력으로 설정
  }
 
void loop(){
    if(조건)
    {
    digitalWrite(Relay,HIGH);     // 조건 만족하면 1채널 릴레이 ON
    delay(500);
    }
    else                               
    {
    digitalWrite(Relay,LOW);      // 아니면 1채널 릴레이 OFF
    delay(500);
    }
}
//출처: https://it-g-house.tistory.com/entry/Arduino-아두이노-릴레이-Relay-스위치-사용방법 [IT-G-House:티스토리]
