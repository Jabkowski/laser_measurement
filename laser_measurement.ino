// =====Laser ranger M01 (6 pins) – Minimal Q/L/K/R =====
// SPDX-License-Identifier: MIT 
// Copyright (c) 2025 Andres Ros
// ESP32 Wemos Lolin32
// Laser ranging sensor 50m (Liancheng Electronics (Shenzhen) Co., Ltd. Store) AliExpress
// LOLIN32/ESP32: Module TX (yellow) -> RX ESP32 (GPIO33)
//                Module RX (green)  <- TX ESP32 (GPIO32)
//                ENA -> GPIO5 (high), MIN->3V3, GND->GND
// L=Turn Laser ON, Q= quick reading, K= Turn Laser OFF, R= reset module
// measurement in meters with two decimals

#include <Arduino.h>

// --- Pins (adjust if you use others) ---
static const int PIN_RX  = 17;   // Module TXD -> RX ESP32
static const int PIN_TX  = 16;   // Module RXD <- TX ESP32
static const int PIN_ENA = 5;    // ENA (HIGH = active)

// --- UART ---
HardwareSerial LZR(2);

// --- Commands ---
const uint8_t CMD_LASER_ON[]  = {0xAA,0x00,0x01,0xBE,0x00,0x01,0x00,0x01,0xC1};
const uint8_t CMD_LASER_OFF[] = {0xAA,0x00,0x01,0xBE,0x00,0x01,0x00,0x00,0xC0};
const uint8_t CMD_QUICK[]     = {0xAA,0x00,0x00,0x22,0x00,0x01,0x00,0x00,0x23};
const uint8_t CMD_READ_RES[]  = {0xAA,0x80,0x00,0x22,0xA2};

// --- State ---
float lastMeters = NAN;

// --- Utilities ---
static inline void enaHigh(){ pinMode(PIN_ENA,OUTPUT); digitalWrite(PIN_ENA,HIGH); }
static inline void enaLow(){  pinMode(PIN_ENA,OUTPUT); digitalWrite(PIN_ENA,LOW);  }
static inline void powerCycle(){ enaLow(); delay(120); enaHigh(); delay(400); }

static inline bool csumOK(const uint8_t* f,int n){
  if(n<3) return false; uint32_t s=0; for(int i=1;i<n-1;i++) s+=f[i]; return ((uint8_t)s)==f[n-1];
}
static inline uint32_t bcd32(const uint8_t* b){
  uint32_t v=0; for(int i=0;i<4;i++){ v=v*100 + ((b[i]>>4)&0x0F)*10 + (b[i]&0x0F); } return v;
}

// Reads for 'ms' milliseconds and, if it finds a valid frame (13B), prints meters and returns true
bool readMeters(unsigned long ms){
  uint8_t f[16]; int pos=0; unsigned long t0=millis(), last=0; bool any=false;
  while(millis()-t0 < ms){
    while(LZR.available()){
      uint8_t x=LZR.read();
      if(pos==0 && x!=0xAA) continue;
      f[pos++]=x; last=millis();
      if(pos==13){
        if(f[0]==0xAA && f[4]==0x00 && f[5]==0x04 && csumOK(f,13)){
          uint8_t func=f[3];
          if(func==0x20 || func==0x21 || func==0x22){
            lastMeters = bcd32(&f[6]) / 1000.0f;
            Serial.println(lastMeters, 3);
            any = true;
          }
        }
        pos=0;
      }
      if(pos>=13) pos=0;
    }
    if(pos>0 && last && millis()-last>80){ pos=0; last=0; }
  }
  return any;
}

// --- Actions ---
void doLaserOn(){  LZR.write(CMD_LASER_ON,  sizeof(CMD_LASER_ON));  LZR.flush(); }
void doLaserOff(){ LZR.write(CMD_LASER_OFF, sizeof(CMD_LASER_OFF)); LZR.flush(); }

void doQuick(){
  while(LZR.available()) LZR.read();           // clear RX
  LZR.write(CMD_QUICK, sizeof(CMD_QUICK)); LZR.flush();
  if(!readMeters(1000)){                       // wait for frame
    LZR.write(CMD_READ_RES, sizeof(CMD_READ_RES)); LZR.flush(); // try last result
    readMeters(200);
  }
}

void doReset(){                                // R = "clear/reset"
  powerCycle();                                 // only resets the module
  while(LZR.available()) LZR.read();            // clear RX
  lastMeters = NAN;
  Serial.println("OK RESET");
}

// --- Setup / Loop ---
void setup(){
  Serial.begin(115200);
  enaHigh();
  LZR.begin(9600, SERIAL_8N1, PIN_RX, PIN_TX);

  Serial.println("\nM01 ready: Q=measure  L=laser ON  K=laser OFF  R=reset");
}

void loop(){
  if(Serial.available()){
    char c = toupper((unsigned char)Serial.read());
    if(c=='Q') doQuick();
    else if(c=='L') doLaserOn();
    else if(c=='K') doLaserOff();
    else if(c=='R') doReset();
  }
  // in case frames arrive (e.g., continuous mode activated externally)
  if(LZR.available()) readMeters(200);
}
