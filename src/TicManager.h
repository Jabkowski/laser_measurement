#ifndef TIC_MANAGER_H
#define TIC_MANAGER_H

#include "Tic.h"
#include <Wire.h>

class TicManager {
public:
    TicManager(uint8_t address) : tic(address) {}

    void begin() {
        Wire.begin();
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

private:
    TicI2C tic;
};

#endif // TIC_MANAGER_H
