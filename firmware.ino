#include <Arduino.h>

#define RXD1 4
#define TXD1 15
#define DWIN_HEADER_H 0x5A
#define DWIN_HEADER_L 0xA5

uint8_t rcvBuffer[64]; 
uint8_t bufIndex = 0;
bool packetStarted = false;
uint8_t expectedLength = 0;

const uint8_t target_addr_high = 0x15;
const uint8_t target_addr_low  = 0x00;


bool checkSyncAndReadData(uint8_t add_higher, uint8_t add_lower, uint8_t* outputContainer, int payload_index, int payload_size) {
  
  while (Serial1.available()) {
    uint8_t b = Serial1.read();
    if (!packetStarted) {
      if (bufIndex == 0 && b == DWIN_HEADER_H) {
        rcvBuffer[bufIndex++] = b;
      } else if (bufIndex == 1 && b == DWIN_HEADER_L) {
        rcvBuffer[bufIndex++] = b;
        packetStarted = true;
      } else {
        bufIndex = 0; // Reset if sequence breaks
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
           outputContainer[i] = rcvBuffer[payload_index + i];
        }
      }
      bufIndex = 0;
      packetStarted = false;
      return true; 
    }
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, RXD1, TXD1);
  Serial.println("System Started");
}

void loop() {
  uint8_t extractedPayload[4]; 
  if (checkSyncAndReadData(target_addr_high, target_addr_low, extractedPayload, 7, 4)) {
    Serial.print("Payload Extracted: ");
    for(int i=0; i<4; i++){
      Serial.printf("%02X ", extractedPayload[i]);
    }
    Serial.println();
  }

  
}