#ifndef __AR4BR_BT_H
#define __AR4BR_BT_H

#include "Aranet4.h"
#include "utils.h"

#define DEVICE_NAME_MAX_LEN  18

class MyAranet4Callbacks: public Aranet4Callbacks {
    uint32_t pin = -1;
    bool enPairing = true;
    bool pairDenied = false;

    uint32_t onPinRequested() {
        if (!enPairing) {
            Serial.println("PIN Requested, but pairing is disabled.");
            pairDenied = true;
            return 123456;
        }

        Serial.println("PIN Requested. Waiting for web or timeout");
        long timeout = millis() + 30000;
        while (pin == -1 && timeout > millis())
            vTaskDelay(500 / portTICK_PERIOD_MS);

        uint32_t ret = pin;
        pin = -1;

        return  ret;
    }
public:
    bool providePin(uint32_t newpin) {
        if (pin == -1) {
            pin = newpin;
            return true;
        }

        return false;
    }

    void disablePairing() {
        enPairing = false;
    }

    void enablePairing() {
        enPairing = true;
    }

    bool pairWasdenied() {
        bool tmp = pairDenied;
        pairDenied = false;
        return tmp;
    }
};

#endif
