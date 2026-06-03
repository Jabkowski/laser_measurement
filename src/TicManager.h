#ifndef TIC_MANAGER_H
#define TIC_MANAGER_H

#include "Tic.h"
#include <Wire.h>

class TicManager {
public:
    TicManager(uint8_t address) : tic(address), address(address) {}

    void begin() {
        // Explicitly initialize Wire on the ESP32 default I2C pins (SDA=21, SCL=22)
        // This avoids ambiguity on some ESP32 boards where defaults may vary.
        Wire.begin(21, 22);
        Wire.setClock(400000); // try fast mode; change to 100000 if needed
        delay(50);

        Serial.println("TicManager: Scanning I2C bus for devices...");
        bool found = false;
        for (uint8_t addr = 1; addr < 127; ++addr) {
            Wire.beginTransmission(addr);
            uint8_t rc = Wire.endTransmission();
            if (rc == 0) {
                Serial.print("  Found device at 0x");
                if (addr < 16) Serial.print('0');
                Serial.print(addr, HEX);
                Serial.println();
                if (addr == address) found = true;
            }
            delay(1);
        }

        if (!found) {
            Serial.print("Warning: Tic at address 0x");
            if (address < 16) Serial.print('0');
            Serial.print(address, HEX);
            Serial.println(" not found on I2C bus.");
            Serial.println(" - Check Tic power (motor V+ and logic power) and common GND.");
            Serial.println(" - Ensure SDA (ESP32 GPIO21) -> Tic SDA, SCL (ESP32 GPIO22) -> Tic SCL.");
            Serial.println(" - Ensure proper pull-ups on SDA/SCL (4.7k recommended). If Tic is powered at 5V,");
            Serial.println("   the bus will be pulled to 5V unless a level shifter or 3.3V pull-ups are used.");
            Serial.println(" - If you see devices at other addresses, check Tic address configuration/jumpers.");
            // Do not attempt to call tic.exitSafeStart() if device not found.
            return;
        }

        Serial.println("Tic found. Initializing...");
        // Safe to call Tic commands now
        tic.exitSafeStart();
    }

    void deenergize() {
        tic.deenergize();
    }

    void energize() {
        tic.energize();
    }

    void goHomeReverse() {
        tic.goHomeReverse();
    }

    void goHomeForward() {
        tic.goHomeForward();
    }

    void setTargetVelocity(int32_t velocity) {
        tic.setTargetVelocity(velocity);
    }

    void setTargetPosition(int32_t position) {
        tic.setTargetPosition(position);
    }

    void setCurrentLimit(uint16_t mA) {
        tic.setCurrentLimit(mA);
    }

    void setStepMode(TicStepMode mode) {
        tic.setStepMode(mode);
    }

    void commandTimeout() {
        tic.resetCommandTimeout();
    }

    void clearDriverError() {
        tic.clearDriverError();
    }

private:
    TicI2C tic;
    uint8_t address;
};

#endif // TIC_MANAGER_H
