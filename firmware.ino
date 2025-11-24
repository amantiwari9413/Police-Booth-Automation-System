#include <Arduino.h>

#define RXD1 4
#define TXD1 15
#define DWIN_HEADER_H 0x5A
#define DWIN_HEADER_L 0xA5
#define GATE 33
#define LIGHT_INPUT 26
#define LIGHT_OUTPUT 33
#define ALARM_INPUT 25
#define ALARM_OUTPUT 12 
#define GATE_INPUT 32
#define GATE_OUTPUT 13


uint8_t rcvBuffer[64];
uint8_t bufIndex = 0;
bool packetStarted = false;
uint8_t expectedLength = 0;
uint8_t extractedPayload[4];
const uint8_t target_addr_high = 0x15;
const uint8_t target_addr_low = 0x00;
uint8_t password[4] = {0x00,0x00,0x04,0xD2};
uint8_t master_password[4] = { 0x49, 0x96, 0x7B, 0x62 }; //1234598754 
bool isMaster = false;

bool checkSyncAndReadPayload(uint8_t add_higher, uint8_t add_lower, uint8_t* payloadContainer, int payload_index, int payload_size) {

  while (Serial1.available()) {
    uint8_t b = Serial1.read();
    if (!packetStarted) {
      if (bufIndex == 0 && b == DWIN_HEADER_H) {
        rcvBuffer[bufIndex++] = b;
      } else if (bufIndex == 1 && b == DWIN_HEADER_L) {
        rcvBuffer[bufIndex++] = b;
        packetStarted = true;
      } else {
        bufIndex = 0;
      }
      continue;
    }
    rcvBuffer[bufIndex++] = b;

    if (bufIndex == 3) {
      expectedLength = rcvBuffer[2] + 3;
      if (expectedLength > 64) {
        bufIndex = 0;
        packetStarted = false;
      }
    }
    if (bufIndex == 5) {
      if (rcvBuffer[4] != add_higher) {
        bufIndex = 0;
        packetStarted = false;
      }
    }
    if (bufIndex == 6) {
      if (rcvBuffer[5] != add_lower) {
        bufIndex = 0;
        packetStarted = false;
      }
    }

    if (packetStarted && bufIndex >= expectedLength) {

      Serial.println("Received Packet!");
      for (int i = 0; i < payload_size; i++) {
        if ((payload_index + i) < expectedLength) {
          payloadContainer[i] = rcvBuffer[payload_index + i];
          Serial.printf("%02X ", payloadContainer[i]);
        }
      }
      bufIndex = 0;
      packetStarted = false;
      return true;
    }
  }
  return false;
}

bool checkPassWord(uint8_t* payload, int size, uint8_t* password) {

  for (int i = 0; i < size; i++) {
    if (payload[i] != password[i]) {
      Serial.println("worng password");
      return false;
    }
  }
  return true;
}


void changePage(uint16_t pageID) {
  byte command[10];
  command[0] = 0x5A;
  command[1] = 0xA5;
  command[2] = 0x07;
  command[3] = 0x82;
  command[4] = 0x00;
  command[5] = 0x84;
  command[6] = 0x5A;
  command[7] = 0x01;
  command[8] = (pageID >> 8) & 0xFF;
  command[9] = pageID & 0xFF;
  Serial1.write(command, 10);
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, RXD1, TXD1);
  Serial.println("System Started");
  changePage(0);
  pinMode(LIGHT_INPUT,INPUT);
  pinMode(LIGHT_OUTPUT,OUTPUT);
  pinMode(ALARM_INPUT,INPUT);
  pinMode(ALARM_OUTPUT,OUTPUT);
  pinMode(GATE_INPUT,INPUT);
  pinMode(GATE_OUTPUT,OUTPUT);
}

void loop() {

  if (Serial1.available() > 0 && (isMaster == false) ) {
    if (checkSyncAndReadPayload(target_addr_high, target_addr_low, extractedPayload, 7, 4)) {
      Serial.print("isMaster : ");
      Serial.println(isMaster);
      if(checkPassWord(extractedPayload,4,password)){
        Serial.println("password correct !");
        delay(1000);
        digitalWrite(GATE_OUTPUT,1);
        changePage(0);
      }else if(checkPassWord(extractedPayload,4,master_password)){
        isMaster=true;
        changePage(9);
      }else{
        changePage(6);
      }
    }
  }

  if(Serial1.available() > 0 && isMaster){
    Serial.println("in the change block ");
    if(checkSyncAndReadPayload(0x30, 0x00, extractedPayload, 7, 4)){
      for(int i=0 ; i < 4; i++){
        password[i]= extractedPayload[i]; 
      }
      isMaster = false;
      delay(1000);
      changePage(0);
    } 
  }

  int gate = digitalRead(GATE_INPUT);
  int light = digitalRead(LIGHT_INPUT);
  int alarm =digitalRead(ALARM_INPUT);

  if(gate){
    digitalWrite(GATE_OUTPUT,1);
  }else{
    digitalWrite(GATE_OUTPUT,0);
  }

  if(light){
    digitalWrite(LIGHT_OUTPUT,1);
  }else{
    digitalWrite(LIGHT_OUTPUT,0);
  }

  if(alarm){
    digitalWrite(ALARM_OUTPUT,1);
  }else{
    digitalWrite(ALARM_OUTPUT,0);
  }

  delay(100);
}