#ifndef TIC_MANAGER_H
#define TIC_MANAGER_H

#include "Tic.h"
#include <Wire.h>

enum RequestType {
    REQUEST_NONE = 0,
    REQUEST_MOVE,
    REQUEST_CONTINUOUS,
    REQUEST_STOP = 0xFF
};

class TicManager {
public:
    // Create manager for Tic at given I2C address
    TicManager(uint8_t address)
      : tic(address), address(address), m_ready(false), m_state(STATE_IDLE),
        m_requested(REQUEST_NONE), m_activeTarget(0), m_energized(true), m_start_position(0),
        m_lastKeepAliveMs(0), m_runStartMs(0), m_runTimeoutMs(DEFAULT_RUN_TIMEOUT_MS) {}

    // Initialize I2C and detect Tic on the bus. Safe to call in setup().
    void begin() {
        Wire.begin(21, 22); // explicit SDA=21, SCL=22 for ESP32
        Wire.setClock(400000);
        delay(50);

        Serial.println("TicManager: Scanning I2C bus for devices...");
        m_ready = false;
        for (uint8_t addr = 1; addr < 127; ++addr) {
            Wire.beginTransmission(addr);
            uint8_t rc = Wire.endTransmission();
            if (rc == 0) {
                Serial.print("  Found device at 0x");
                if (addr < 16) Serial.print('0'); 
                Serial.print(addr, HEX); Serial.println();
                if (addr == address) m_ready = true;
            }
            delay(1);
        }
        if (!m_ready) {
            Serial.print("Warning: Tic at address 0x"); 
            Serial.print(address, HEX); Serial.println(" not found on I2C bus.");
            return;
        }

        Serial.println("TicManager: Tic found. Exiting safe start (if needed).");
        tic.exitSafeStart();
        m_start_position= tic.getTargetPosition();
        Serial.print("TicManager: start position=");
        Serial.println(m_start_position);
        m_state_prev = m_state;
    }

    void Run() {
        if (!m_ready) return; // bus not initialized or Tic not present

        unsigned long now = millis();
        bool should_keep_alive = now - m_lastKeepAliveMs >= KEEPALIVE_MS;
        if ( m_state_prev != m_state) {
            Serial.print("TicManager:state: ");
            Serial.println(m_state);
        }
        switch (m_state) {
            case STATE_IDLE:
            {
                if (m_energized) {
                    tic.deenergize();
                    m_energized = false;
                    Serial.println("TicManager:requested -> Deenergized");
                }
                // If a move was requested, start it
                if (should_keep_alive) {
                    // If we have been idle for a while, reset Tic command timeout to avoid stale state
                    tic.resetCommandTimeout();
                    m_lastKeepAliveMs = now;
                }
                if (m_requested != REQUEST_NONE) {
                    Serial.println("TicManager:requested -> starting");
                    // Start move
                    m_state = STATE_START;
                }
                m_state_prev = m_state;
                break;
            }
            case STATE_START:
            {
                Serial.println("TicManager: STATE_START");
                // Energize and issue first target
                tic.exitSafeStart();
                delay(2);
                tic.energize();
                m_energized = true;
                m_runStartMs = now;

                if (m_requested == REQUEST_MOVE) {
                    tic.setTargetPosition(m_activeTarget);
                    Serial.print("TicManager: move start, target=");
                    Serial.println(m_activeTarget);
                    m_state = STATE_RUNNING;
                }
                else if (m_requested == REQUEST_CONTINUOUS) {
                    m_activeTarget = tic.getTargetPosition();
                    Serial.print("TicManager: continuous start, step=");
                    Serial.println(m_continuousStep);
                    m_state = STATE_CONTINUOUS;
                }
                else if (m_requested == REQUEST_STOP) {
                    Serial.println("TicManager: stop requested");
                    m_state = STATE_FINISH;
                }
                else if (m_requested == REQUEST_NONE) {
                    Serial.println("TicManager: no new request, go to finish");
                    m_state = STATE_FINISH;
                }
                m_state_prev = m_state;
                break;
            }
            case STATE_RUNNING:
            {   
                // int16_t cur_pos = tic.getPosition();
                // Serial.print("TicManager: running, current position=");
                // Serial.println(cur_pos);
                if(tic.getTargetPosition() == m_activeTarget) {
                    Serial.println("TicManager: target position reached");
                    m_state = STATE_FINISH;
                    m_requested = REQUEST_NONE;
                }
                m_state_prev = m_state;
                break;
            }
            case STATE_CONTINUOUS:
            {
                // If user requested a new move while running, switch target immediately
                m_activeTarget = clampToLimits(m_activeTarget);
                tic.setTargetPosition(m_activeTarget);
                m_lastKeepAliveMs = now;
                m_runStartMs = now; // restart run timeout
                m_state_prev = m_state;
                break;
            }
            case STATE_FINISH:
            {
                // Deenergize and go to IDLE
                if (m_energized) {
                    tic.deenergize();
                    m_energized = false;
                }
                m_state = STATE_IDLE;
                Serial.println("TicManager: finished, deenergized");
                m_state_prev = m_state;
                break;
            }
        }
    }

    // Helper: clamp value to limits if enabled
    int32_t clampToLimits(int32_t v) const {
        if (!m_hasLimits) return v;
        if (v < m_minPos) return m_minPos;
        if (v > m_maxPos) return m_maxPos;
        return v;
    }

    void requestMove(RequestType type, int32_t target) {
        if(type == REQUEST_NONE) return; // ignore
        else if(type == REQUEST_STOP) {
            Serial.print("TicManager: move stop requested");
            m_state = STATE_FINISH;
        }
        else if(type == REQUEST_CONTINUOUS) {
            Serial.print("TicManager: move continuous, step=");
            Serial.println(target);
            m_continuousStep = target; // in continuous mode, target is step size
        }
        else if(type == REQUEST_MOVE) {
            Serial.print("TicManager: move requested, target=");
            Serial.println(target);
            // clamp target to limits immediately to avoid issues in Run() when accepting new target
            target = clampToLimits(target);
            m_activeTarget = target;
        }
        m_requested = type;
    }
    
    void doStep(){
        if (m_state == STATE_CONTINUOUS) {
            // Update active target position by step and issue new command
            m_activeTarget+= m_continuousStep;
             Serial.println("TicManager: continuous step ++");
        }
    }
    void setLimits(int32_t minPos, int32_t maxPos) {
        if (minPos > maxPos) { 
            Serial.println("TicManager: warning: setLimits minPos > maxPos, WRONG USAGE! Swapping values.");
            int32_t t = minPos;
            minPos = maxPos;
            maxPos = t;
        }
        m_minPos = minPos;
        m_maxPos = maxPos;
        m_hasLimits = true;
    }
    
    // End continuous stepping; will deenergize (STATE_FINISH) on next Run() loop.
    void endContinuous() {
        if (m_state == STATE_CONTINUOUS) {
            m_activeTarget = m_start_position;
            m_requested = REQUEST_MOVE;
            tic.setTargetPosition(m_activeTarget);
            m_state = STATE_RUNNING;
            Serial.println("TicManager: continuous end requested, returning to start position");
        }
    }
private:
    // Internal constants
    static const unsigned long KEEPALIVE_MS = 500; // how often to resend position to reset Tic timeout
    static const unsigned long DEFAULT_RUN_TIMEOUT_MS = 60000; // 60s default auto-finish

    enum State {
        STATE_IDLE = 0,
        STATE_START,
        STATE_CONTINUOUS,
        STATE_RUNNING,
        STATE_FINISH,
        STATE_ERROR
    };
    TicI2C tic;
    uint8_t address;

    bool m_ready;
    State m_state;
    State m_state_prev;
    RequestType m_requested;

    // active run
    int32_t m_activeTarget;
    int32_t m_start_position;
    bool m_energized;
    unsigned long m_lastKeepAliveMs;
    unsigned long m_runStartMs;
    unsigned long m_runTimeoutMs;
    // continuous mode
    int32_t m_continuousStep;
    // optional limits
    bool m_hasLimits = false;
    int32_t m_minPos = 0;
    int32_t m_maxPos = 0;
    // flag to avoid spamming limit reached messages
    bool m_continuousAtLimit = false;
};

#endif // TIC_MANAGER_H