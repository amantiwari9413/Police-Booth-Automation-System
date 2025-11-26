#include <Arduino.h>
#define RXD1 4
#define TXD1 15
#define DWIN_HEADER_H 0x5A
#define DWIN_HEADER_L 0xA5

#define LIGHT_INPUT 26  //  brown wire    no light = 1   && light = 0
#define LIGHT_OUTPUT 13

#define ALARM_INPUT 27  // white wire   vibratio =0
#define ALARM_OUTPUT 12 

#define INER_LIGHT 25     //  gray wire
#define INER_LIGHT_OUTPUT 33

#define GATE_OUTPUT 37

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

class DigitalMode {
  private:
    int pin;
    unsigned long sampleInterval;
    int numSamples;

    unsigned long lastSampleTime = 0;
    int sampleIndex = 0;
    int highCount = 0;
    int lowCount = 0;

    bool resultReady = false;
    bool result = false;

  public:
    DigitalMode(int pin, unsigned long interval, int samples = 7)
      : pin(pin), sampleInterval(interval), numSamples(samples) {
      pinMode(pin, INPUT_PULLUP);
    }
    bool update() {
      unsigned long now = millis();

      if (now - lastSampleTime < sampleInterval) {
        return false;
      }

      lastSampleTime = now;
      int reading = digitalRead(pin);
      if (reading == HIGH) highCount++;
      else                lowCount++;
      sampleIndex++;
      if (sampleIndex >= numSamples) {
        result = (highCount > lowCount);
        sampleIndex = 0;
        highCount = 0;
        lowCount = 0;
        resultReady = true;
      }
      return resultReady;
    }
    bool getResult() {
      resultReady = false;   
      return result;
    }
};

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
//sensor object 

DigitalMode getInerLight(INER_LIGHT,5,7);
DigitalMode getOuterLight(LIGHT_INPUT,20,10);
DigitalMode getAlarm(ALARM_INPUT,5,10);


void setup() {
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, RXD1, TXD1);
  Serial.println("System Started");
  changePage(0);
  
  pinMode(LIGHT_OUTPUT,OUTPUT);
  pinMode(INER_LIGHT_OUTPUT,OUTPUT);
  pinMode(GATE_OUTPUT,OUTPUT);
  pinMode(ALARM_OUTPUT,OUTPUT);
}

void loop() {

  if (Serial1.available() > 0 && !isMaster ) {  
    if (checkSyncAndReadPayload(target_addr_high, target_addr_low, extractedPayload, 7, 4)) {
      Serial.print("isMaster : ");
      Serial.println(isMaster);
      if(checkPassWord(extractedPayload,4,password)){
        Serial.println("password correct !");
        digitalWrite(GATE_OUTPUT,0);
        delay(1000);
        digitalWrite(GATE_OUTPUT,1);
        delay(1000);
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

   // INNER light filter
  if (getInerLight.update()) {
    bool state = getInerLight.getResult();
    Serial.print("Inner Light: ");
    Serial.println(state);
    digitalWrite(INER_LIGHT_OUTPUT, state);
  }

  // OUTER light filter
  if (getOuterLight.update()) {
    bool state = getOuterLight.getResult();
    Serial.print("Outer Light: ");
    Serial.println(state);
    digitalWrite(LIGHT_OUTPUT, state);
  }

    // OUTER light filter
  if (getAlarm.update()) {
    bool state = getAlarm.getResult();
    Serial.print("Alarm : ");
    Serial.println(!state);
    digitalWrite(ALARM_OUTPUT, !state);
  }

}